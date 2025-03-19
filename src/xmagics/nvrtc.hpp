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
        int loadIncludes();
        int defineCUDACheckError();
        int declareNVRTCVar();
        int definePTX(const std::string& code);

        cling::Interpreter& m_interpreter;
        bool initializationDone=false;
        //xcpp::interpreter& x_interpreter;
        
    };
}  


#endif