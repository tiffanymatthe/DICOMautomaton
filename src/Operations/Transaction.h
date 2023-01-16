// Transaction.h.

#pragma once

#include <map>
#include <string>

#include "../Structs.h"


OperationDoc OpArgDocTransaction();

bool Transaction(Drover &DICOM_data,
                 const OperationArgPkg& /*OptArgs*/,
                 std::map<std::string, std::string>& /*InvocationMetadata*/,
                 const std::string& /*FilenameLex*/);
