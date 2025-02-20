//RPCReceive.cc - A part of DICOMautomaton 2022. Written by hal clark.

#include <any>
#include <optional>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <string>    
#include <vector>

#include "YgorMisc.h"
#include "YgorLog.h"
#include "YgorMath.h"         //Needed for vec3 class.
#include "YgorStats.h"
#include "YgorString.h"       //Needed for GetFirstRegex(...)
#include "YgorImages.h"

#include "../Structs.h"
#include "../Regex_Selectors.h"

#ifndef DCMA_USE_THRIFT
    #error "Attempted to compile RPC server without Apache Thrift, which is required"
#endif //DCMA_USE_THRIFT

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

#include "../rpc/gen-cpp/Receiver.h"
#include "../rpc/Serialization.h"

#include "RPCReceive.h"


using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

class ReceiverHandler : virtual public ::dcma::rpc::ReceiverIf {
  public:
    ReceiverHandler() {
        YLOGINFO("Initialization code goes here");
    }

    void GetSupportedOperations(std::vector<::dcma::rpc::KnownOperation> & _return,
                                const ::dcma::rpc::OperationsQuery& query) {
        YLOGINFO("GetSupportedOperations implementation goes here");

        // Simple test.
        {
            ::vec3<double> a;
            ::dcma::rpc::vec3_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Slightly more complicated test.
        {
            ::contour_of_points<double> a;
            ::dcma::rpc::contour_of_points_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Slightly more complicated test.
        {
            ::contour_collection<double> a;
            ::dcma::rpc::contour_collection_double b;
            Serialize(a, b);
            Deserialize(b, a);
        }

        // Full-blown test of Drover serialization.
        {
            ::Drover a;
            ::dcma::rpc::Drover b;
            Serialize(a, b);
            Deserialize(b, a);
            Serialize(a, b);
        }
    }

    void LoadFiles(::dcma::rpc::LoadFilesResponse& _return,
                   const std::vector<::dcma::rpc::LoadFilesQuery> & server_filenames) {
        YLOGINFO("LoadFiles implementation goes here");
    }
};


OperationDoc OpArgDocRPCReceive(){
    OperationDoc out;
    out.name = "RPCReceive";

    out.desc = 
        "This operation launches a server that accepts remote procedure calls (RPC) for distributed computing.";

    out.notes.emplace_back(
        "RPC functionality is currently alpha-quality code, and much is expected to change."
    );

    out.args.emplace_back();
    out.args.back().name = "Port";
    out.args.back().desc = "The port number to listen on.";
    out.args.back().default_val = "9090";
    out.args.back().expected = true;
    out.args.back().examples = { "13", "8080", "9090", "16378" };

    return out;
}



bool RPCReceive(Drover &DICOM_data,
                     const OperationArgPkg& OptArgs,
                     std::map<std::string, std::string>& /*InvocationMetadata*/,
                     const std::string& /*FilenameLex*/){

    //---------------------------------------------- User Parameters --------------------------------------------------
    const auto Port = std::stol( OptArgs.getValueStr("Port").value() );
    //-----------------------------------------------------------------------------------------------------------------

    //using namespace ::dcma::rpc;

    auto handler = std::make_shared<ReceiverHandler>();
    auto processor = std::make_shared<::dcma::rpc::ReceiverProcessor>(handler);

    auto transport_server = std::make_shared<TServerSocket>( Port );
    auto transport_factory = std::make_shared<TBufferedTransportFactory>();
    auto protocol_factory = std::make_shared<TBinaryProtocolFactory>();

    TSimpleServer server(processor, transport_server, transport_factory, protocol_factory);
    server.serve();

    return true;
}

