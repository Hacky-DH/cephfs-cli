%module cephfstool
%include "std_string.i"
%include "std_vector.i"
%template(StringVector) std::vector<std::string>;
%{
#include "cephfstool.h"
%}
%include "cephfstool.h"
