#include <algorithm>
#include <cmath>
#include <cstdlib>            //Needed for exit() calls.
#include <fstream>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <set> 
#include <stdexcept>
#include <string>    
#include <random>

#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorMathIOOBJ.h"
#include "YgorMisc.h"         //Needed for FUNCINFO, FUNCWARN, FUNCERR macros.

#include "Explicator.h"

#include "../Contour_Boolean_Operations.h"
#include "../Structs.h"
#include "../Regex_Selectors.h"

#include "../Simple_Meshing.h"
#include "../Surface_Meshes.h"

#include "GetHausdorffDistance.h"

OperationDoc OpArgDocGetHausdorffDistance(){
    OperationDoc out;
    out.name = "GetHausdorffDistance";

    out.desc = 
        "This routine calculates the hausdorff distance between two meshes and prints it to the terminal output."
        ;


    out.args.emplace_back();
    out.args.back() = SMWhitelistOpArgDoc();
    out.args.back().name = "MeshSelection";
    out.args.back().default_val = "all";

    return out;
}

bool GetHausdorffDistance(Drover &DICOM_data,
                               const OperationArgPkg& OptArgs,
                               std::map<std::string, std::string>& /*InvocationMetadata*/,
                               const std::string& FilenameLex){

    Explicator X(FilenameLex);

    FUNCINFO("In get HAUSDORFF distance.");

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto MeshSelectionStr = OptArgs.getValueStr("MeshSelection").value();

    //-----------------------------------------------------------------------------------------------------------------
    auto SMs_all = All_SMs( DICOM_data );
    auto SMs = Whitelist( SMs_all, MeshSelectionStr );

    long int completed = 0;
    const auto sm_count = SMs.size();
    const int num_meshes = 2;
    int num_to_skip = sm_count - num_meshes;

    if (sm_count < num_meshes) {
        FUNCERR("Must select at least two meshes.");
    }
    if (sm_count > num_meshes) {
        FUNCWARN("Can only calculate the HausdorffDistance of two meshes, currently have " << sm_count << " meshes selected. Only looking at last two.");
    }

    for(auto & smp_it : SMs){
        if (num_to_skip != 0) {
            num_to_skip--;
            continue;
        }
        auto mesh = (*smp_it)->meshes;
        FUNCINFO("Found a mesh with the following metadata");
        for(const auto& elem : mesh.metadata)
        {
            FUNCINFO(elem.first << " " << elem.second << "\n");
        }
    }

    return true;
}
