//PointSeparation.cc - A part of DICOMautomaton 2020. Written by hal clark.

#include <any>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>    
#include <vector>

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/Contour_Similarity.h"
#include "PointSeparation.h"
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorString.h"       //Needed for GetFirstRegex(...)


OperationDoc OpArgDocPointSeparation(void){
    OperationDoc out;
    out.name = "PointSeparation";
    out.desc = 
        "This operation estimates the minimum and maximum point-to-point separation between two point clouds."
        " It also computes the longest-nearest (Hausdorff) separation, i.e., the length of the longest lines from points in"
        " selection A to the nearest point in selection B.";

    out.notes.emplace_back(
        "This routine scales like $O(N*M)$ where $N$ and $M$ are the number of points in each point cloud."
        " No indexing or acceleration is used. Beware that large point clouds will result in slow computations."
    );
    out.notes.emplace_back(
        "This operation can be used to compare points clouds that are nearly alike."
        " Such comparisons are useful for quantifying discrepancies after transformations, reconstructions,"
        " simplifications, or any other scenarios where a point cloud must be reasonable accurately"
        " reproduced."
    );

    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelectionA";
    out.args.back().default_val = "first";


    out.args.emplace_back();
    out.args.back() = PCWhitelistOpArgDoc();
    out.args.back().name = "PointSelectionB";
    out.args.back().default_val = "last";


    out.args.emplace_back();
    out.args.back().name = "FileName";
    out.args.back().desc = "A filename (or full path) in which to append separation data generated by this routine."
                           " The format is CSV. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back().name = "UserComment";
    out.args.back().desc = "A string that will be inserted into the output file which will simplify merging output"
                           " with differing parameters, from different sources, or using sub-selections of the data.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "Using XYZ", "Patient treatment plan C" };

    return out;
}

Drover PointSeparation(Drover DICOM_data, OperationArgPkg OptArgs, std::map<std::string,std::string> /*InvocationMetadata*/, std::string FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto PointSelectionAStr = OptArgs.getValueStr("PointSelectionA").value();
    const auto PointSelectionBStr = OptArgs.getValueStr("PointSelectionB").value();

    auto FileName = OptArgs.getValueStr("FileName").value();
    const auto UserComment = OptArgs.getValueStr("UserComment");
    //-----------------------------------------------------------------------------------------------------------------
    Explicator X(FilenameLex);

    auto PCs_all = All_PCs( DICOM_data );
    const auto PCs_A = Whitelist( PCs_all, PointSelectionAStr );
    const auto N_PCs_A = PCs_A.size();
    const auto PCs_B = Whitelist( PCs_all, PointSelectionBStr );
    const auto N_PCs_B = PCs_B.size();

    // Attempt to identify the patient for reporting purposes.
    std::string patient_ID;
    if( auto o = (*PCs_A.front())->pset.GetMetadataValueAs<std::string>("PatientID") ){
        patient_ID = o.value();
    }else if( auto o = (*PCs_B.front())->pset.GetMetadataValueAs<std::string>("PatientID") ){
        patient_ID = o.value();
    }else if( auto o = (*PCs_A.front())->pset.GetMetadataValueAs<std::string>("StudyInstanceUID") ){
        patient_ID = o.value();
    }else if( auto o = (*PCs_B.front())->pset.GetMetadataValueAs<std::string>("StudyInstanceUID") ){
        patient_ID = o.value();
    }else{
        patient_ID = "unknown_patient";
    }

    std::string LabelA;
    if( auto o = (*PCs_A.front())->pset.GetMetadataValueAs<std::string>("Label") ){
        LabelA = o.value();
    }else{
        LabelA = "unknown_pcloud";
    }

    std::string LabelB;
    if( auto o = (*PCs_B.front())->pset.GetMetadataValueAs<std::string>("Label") ){
        LabelB = o.value();
    }else{
        LabelB = "unknown_pcloud";
    }

    size_t N_A = 0; // Total number of points in set A.
    for(const auto & pcpA_it : PCs_A){
        N_A += (*pcpA_it)->pset.points.size();
    }
    size_t N_B = 0; // Total number of points in set B.
    for(const auto & pcpB_it : PCs_B){
        N_B += (*pcpB_it)->pset.points.size();
    }

    // Estimate the separations.
    double sq_separation_min = std::numeric_limits<double>::infinity(); 
    double sq_separation_max = -sq_separation_min;
    double sq_hausdorff = -std::numeric_limits<double>::infinity();

    for(const auto & pcpA_it : PCs_A){
        for(const auto vA : (*pcpA_it)->pset.points){

            double sq_nearest = std::numeric_limits<double>::infinity();
            for(const auto & pcpB_it : PCs_B){
                for(const auto vB : (*pcpB_it)->pset.points){
                    const auto sq_sep = vA.sq_dist(vB);
                    // Identify the nearest point in set B for the current set A point.
                    if(sq_sep < sq_nearest){
                        sq_nearest = sq_sep;
                    }
                    // Identify the shortest A-point to B-point distance for all points in A and B.
                    if(sq_sep < sq_separation_min){
                        sq_separation_min = sq_sep;
                    }
                    // Identify the longest A-point to B-point distance for all points in A and B.
                    if(sq_separation_max < sq_sep){
                        sq_separation_max = sq_sep;
                    }
                }
            }

            // Identify if the nearest matching point in set B for the current set A point is the A-B Hausdorff distance.
            if(sq_nearest < sq_hausdorff){
                sq_hausdorff = sq_nearest;
            }
        }
    }

    // Summarize the findings.
    const auto hausdorff_dist = std::sqrt(sq_hausdorff);

    FUNCINFO("Minimum separation: " << std::sqrt(sq_separation_min));
    FUNCINFO("Maximum separation: " << std::sqrt(sq_separation_max));
    FUNCINFO("Hausdorff separation: " << hausdorff_dist);

    //Report the findings. 
    FUNCINFO("Attempting to claim a mutex");

    {
        //File-based locking is used so this program can be run over many patients concurrently.
        //
        //Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
        boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create,
                                               "dicomautomaton_operation_pointseparation_mutex");
        boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

        if(FileName.empty()){
            FileName = Get_Unique_Sequential_Filename("/tmp/dicomautomaton_pointseparation_", 6, ".csv");
        }
        const auto FirstWrite = !Does_File_Exist_And_Can_Be_Read(FileName);
        std::fstream FO(FileName, std::fstream::out | std::fstream::app);
        if(!FO){
            throw std::runtime_error("Unable to open file for reporting similarity. Cannot continue.");
        }
        if(FirstWrite){ // Write a CSV header.
            FO << "UserComment,"
               << "PatientID,"
               << "LabelA,"
               << "NormalizedLabelA,"
               << "LabelB,"
               << "NormalizedLabelB,"
               << "MinimumSeparation,"
               << "MaximumSeparation,"
               << "HausdorffDistance,"
               << std::endl;
        }
        FO << UserComment.value_or("")     << ","
           << patient_ID                   << ","
           << LabelA                       << ","
           << X(LabelA)                    << ","
           << LabelB                       << ","
           << X(LabelB)                    << ","
           << std::sqrt(sq_separation_min) << ","
           << std::sqrt(sq_separation_max) << ","
           << hausdorff_dist               << ","
           << std::endl;
        FO.flush();
        FO.close();
    }

    return DICOM_data;
}
