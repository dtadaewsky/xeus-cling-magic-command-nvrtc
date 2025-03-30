#ifndef XMAGICS_NVRTC_HPP
#define XMAGICS_NVRTC_HPP





#include "cling/Interpreter/Interpreter.h"
#include "xeus-cling/xmagics.hpp"
#include "xeus-cling/xoptions.hpp"
#include "xeus-cling/xinterpreter.hpp"

#include <string>

namespace xcpp
{
    class nvrtc: public xmagic_cell
    {
    public:

        nvrtc(cling::Interpreter& i) : m_interpreter(i){}
        virtual void operator()(const std::string& line, const std::string& cell) override;
        
    private:
        void generateNVRTC(const std::string& line, const std::string& cell);
        int loadLibrarys();
        int loadIncludes(const std::string includePath);
        int defineCUDACheckError();
        int declareNVRTCVar();
        int definePTX(const std::string& code);
        int initDevice();
        int getDeviceInfo();
        int printDeviceName();
        int getCompileOptions(const std::string& line);
        int getIncludePaths(const std::string& content);

        std::list<std::string> extractFunctionNames(const std::string& ptx);
        std::string removeComments(const std::string& code);
        std::string readFileToString(const std::string& filePath);

        std::string getCudaIncludePath(const std::string line);
        std::string demangle(const std::string& mangled);

        std::vector<std::string> compilerOptions;
        std::vector<std::string> foundHeaders;
        std::vector<std::string> foundContent;
        std::list<std::string> registeredFunctionNames;



        cling::Interpreter& m_interpreter;
        bool initializationDone=false;
        int index=-1;
        int foundCUDADevices=0;
        bool printDeviceInfo=false;

        //xcpp::interpreter& x_interpreter;
        
    };
}  


#endif