//DumpROISNR.cc - A part of DICOMautomaton 2017. Written by hal clark.

#include <cmath>
#include <exception>
#include <any>
#include <optional>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <utility>            //Needed for std::pair.
#include <vector>
#include <filesystem>

#include <boost/interprocess/creation_tags.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "../Structs.h"
#include "../Regex_Selectors.h"
#include "../YgorImages_Functors/Compute/AccumulatePixelDistributions.h"
#include "DumpROISNR.h"
#include "Explicator.h"       //Needed for Explicator class.
#include "YgorFilesDirs.h"    //Needed for Does_File_Exist_And_Can_Be_Read(...), etc..
#include "YgorImages.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.
#include "YgorLog.h"
#include "YgorStats.h"        //Needed for Stats:: namespace.



OperationDoc OpArgDocDumpROISNR(){
    OperationDoc out;
    out.name = "DumpROISNR";

    out.desc = 
        "This operation computes the Signal-to-Noise ratio (SNR) for each ROI. The specific 'SNR' computed is SNR = (mean"
        " pixel) / (pixel std dev) which is the inverse of the coefficient of variation.";
        
        
    out.notes.emplace_back(
        "This routine will combine spatially-overlapping images by summing voxel intensities. So if you have a time"
        " course it may be more sensible to aggregate images in some way (e.g., spatial averaging) prior to calling"
        " this routine."
    );


    out.args.emplace_back();
    out.args.back().name = "SNRFileName";
    out.args.back().desc = "A filename (or full path) in which to append SNR data generated by this routine."
                      " The format is CSV. Leave empty to dump to generate a unique temporary file.";
    out.args.back().default_val = "";
    out.args.back().expected = true;
    out.args.back().examples = { "", "/tmp/somefile", "localfile.csv", "derivative_data.csv" };
    out.args.back().mimetype = "text/csv";


    out.args.emplace_back();
    out.args.back() = NCWhitelistOpArgDoc();
    out.args.back().name = "NormalizedROILabelRegex";
    out.args.back().default_val = ".*";


    out.args.emplace_back();
    out.args.back() = RCWhitelistOpArgDoc();
    out.args.back().name = "ROILabelRegex";
    out.args.back().default_val = ".*";
    
    return out;
}



bool DumpROISNR(Drover &DICOM_data,
                  const OperationArgPkg& OptArgs,
                  std::map<std::string, std::string>& /*InvocationMetadata*/,
                  const std::string& FilenameLex){

    //---------------------------------------------- User Parameters --------------------------------------------------
    auto SNRFileName = OptArgs.getValueStr("SNRFileName").value();
    const auto ROILabelRegex = OptArgs.getValueStr("ROILabelRegex").value();
    const auto NormalizedROILabelRegex = OptArgs.getValueStr("NormalizedROILabelRegex").value();

    //-----------------------------------------------------------------------------------------------------------------

    Explicator X(FilenameLex);

    //Merge the image arrays if necessary.
    if(DICOM_data.image_data.empty()){
        throw std::invalid_argument("This routine requires at least one image array. Cannot continue");
    }

    auto img_arr_ptr = DICOM_data.image_data.front();
    if(img_arr_ptr == nullptr){
        throw std::runtime_error("Encountered a nullptr when expecting a valid Image_Array ptr.");
    }else if(img_arr_ptr->imagecoll.images.empty()){
        throw std::runtime_error("Encountered a Image_Array with valid images -- no images found.");
    }

    //Stuff references to all contours into a list. Remember that you can still address specific contours through
    // the original holding containers (which are not modified here).
    auto cc_all = All_CCs( DICOM_data );
    auto cc_ROIs = Whitelist( cc_all, { { "ROIName", ROILabelRegex },
                                        { "NormalizedROIName", NormalizedROILabelRegex } } );

    if(cc_ROIs.empty()){
        throw std::invalid_argument("No contours selected. Cannot continue.");
    }

    std::string patient_ID;
    if( auto o = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("PatientID") ){
        patient_ID = o.value();
    }else if( auto o = cc_ROIs.front().get().contours.front().GetMetadataValueAs<std::string>("StudyInstanceUID") ){
        patient_ID = o.value();
    }else{
        patient_ID = "unknown_patient";
    }

    //Accumulate the voxel intensity distributions.
    AccumulatePixelDistributionsUserData ud;
    if(!img_arr_ptr->imagecoll.Compute_Images( AccumulatePixelDistributions, { },
                                               cc_ROIs, &ud )){
        throw std::runtime_error("Unable to accumulate pixel distributions.");
    }

    //Report the findings. 
    {
        //File-based locking is used so this program can be run over many patients concurrently.
        //
        //Try open a named mutex. Probably created in /dev/shm/ if you need to clear it manually...
        boost::interprocess::named_mutex mutex(boost::interprocess::open_or_create,
                                               "dcma_op_dumproisnr_mutex");
        boost::interprocess::scoped_lock<boost::interprocess::named_mutex> lock(mutex);

        if(SNRFileName.empty()){
            const auto base = std::filesystem::temp_directory_path() / "dcma_dumproisnr_";
            SNRFileName = Get_Unique_Sequential_Filename(base.string(), 6, ".csv");
        }
        std::fstream FO_snr(SNRFileName, std::fstream::out | std::fstream::app);
        if(!FO_snr){
            throw std::runtime_error("Unable to open file for reporting derivative data. Cannot continue.");
        }
        for(const auto &av : ud.accumulated_voxels){
            const auto lROIname = av.first;
            const auto PixelMean = Stats::Mean( av.second );
            const auto PixelMedian = Stats::Median( av.second );
            const auto PixelStdDev = std::sqrt(Stats::Unbiased_Var_Est( av.second ));

            FO_snr  << "PatientID='" << patient_ID << "',"
                      << "NormalizedROIname='" << X(lROIname) << "',"
                      << "ROIname='" << lROIname << "',"
                      << "PixelMean=" << PixelMean << ","
                      << "PixelMedian=" << PixelMedian << ","
                      << "PixelStdDev=" << PixelStdDev << ","
                      << "SNR=" << PixelMean/PixelStdDev << ","
                      << "VoxelCount=" << av.second.size() << std::endl;
        }
        FO_snr.flush();
        FO_snr.close();
    }

    return true;
}
