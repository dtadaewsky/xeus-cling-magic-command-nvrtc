#include "nvrtc.hpp"

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <list>
#include <functional>
#include <cstdio>
#include "xeus-cling/xoptions.hpp"

#include "../xparser.hpp"

#include <stdexcept>
#include <dlfcn.h>
#include "cling/Interpreter/Interpreter.h"
#include "cling/Interpreter/Value.h"
#include "cling/Interpreter/RuntimeOptions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CodeGen/BackendUtil.h"

#include <unordered_map>
#include <unordered_set>
#include <cstdlib>
#include <ctime>
#include <clang-c/Index.h>

#define ERROR_CODE -1
#define SUCCESS 0



namespace xcpp
{
    void nvrtc::operator()(const std::string& line, const std::string& cell)
    {
        generateNVRTC(line,cell);
    }

    void nvrtc::generateNVRTC(const std::string& line, const std::string& cell)
    {
        if(!initializationDone)
        {   
            if(SUCCESS!=loadLibrarys()) return;
            
            if(SUCCESS!=loadIncludes()) return;
            
            if(SUCCESS!=defineCUDACheckError()) return;
           
            if(SUCCESS!=declareNVRTCVar())  return;
           
            initializationDone=true;    
        }   
        definePTX(cell);


    }

    int nvrtc::loadLibrarys()
    {
        std::string nvrtcLib = "libnvrtc.so";
        std::string cudaLib = "cuda.so";

        if(m_interpreter.loadLibrary(nvrtcLib,true)!=cling::Interpreter::CompilationResult::kSuccess)
        {
            std::cerr << "Could not load library: " << nvrtcLib << std::endl;
            return ERROR_CODE;
        }
        if(m_interpreter.loadLibrary(cudaLib,true)!=cling::Interpreter::CompilationResult::kSuccess)
        {
            std::cerr << "Could not load library: " << cudaLib << std::endl;
            return ERROR_CODE;
        }
        return SUCCESS;
    }

    int nvrtc::loadIncludes()
    {
        std::string nvrtcHeader = "/usr/local/cuda/include/nvrtc.h";    //TODO hier vielleicht ein Automatische suche nach den Daten
        std::string cudaHeader = "/usr/local/cuda/include/cuda.h";
       
        if(m_interpreter.loadHeader(nvrtcHeader)!=cling::Interpreter::CompilationResult::kSuccess)
        {
            std::cerr << "Could not load header: " << nvrtcHeader << std::endl;
            return ERROR_CODE;
        }
        if(m_interpreter.loadHeader(cudaHeader)!=cling::Interpreter::CompilationResult::kSuccess)
        {
            std::cerr << "Could not load header: " << cudaHeader << std::endl;
            return ERROR_CODE;
        }
        return SUCCESS;
    }
    int nvrtc::defineCUDACheckError()
    {
        const std::string clingInput="#define checkCudaError(err) if(err != CUDA_SUCCESS) {std::cerr <<\"CUDA Error:\" << err << std::endl;return -1; }";
        cling::Value output; 
        if(m_interpreter.loadHeader("iostream")!=cling::Interpreter::CompilationResult::kSuccess)
        {
            std::cerr << "Could not load header: " <<  "iostream" << std::endl;
            return ERROR_CODE;
        } 

        if(m_interpreter.process(clingInput, &output)!=cling::Interpreter::CompilationResult::kSuccess)
        {
            std::cerr << "Could not define CudaErrorFunction" << std::endl;
            return ERROR_CODE;
        } 
        return SUCCESS;
    }
    int nvrtc::declareNVRTCVar()
    {
        if(m_interpreter.declare("nvrtcResult result;")!=cling::Interpreter::CompilationResult::kSuccess)
        {
            std::cerr << "Could not declare nvrtcResult" << std::endl;
            return ERROR_CODE;
        } 
        return SUCCESS;
    } 
    int nvrtc::definePTX(const std::string& code)
    {
        cling::Value output;
        std::string clingInput;
        std::string generateProgVarName = "prog"; 
        
        generateProgVarName += "index"; //TODO  Sollte immer einen neuen namen haben, vielleicht auch mit index aus einer Variable
        
        std::string declareInput = "nvrtcProgram " +generateProgVarName + ";";
        
        m_interpreter.declare(declareInput);
        m_interpreter.declare("size_t ptxSize;");


        clingInput= "const char *kernelCodeInCharArrey =R\"RawMarker(" + code + ")RawMarker\";"; //TODO kernelCodeInCharArrey variable to garantie for more then one time useable 
        m_interpreter.process(clingInput, &output);


        clingInput = "nvrtcCreateProgram(&" + generateProgVarName + ", kernelCodeInCharArrey,\"add.cu\",0,nullptr,nullptr);";
        m_interpreter.process(clingInput, &output);
        

        clingInput = "result =nvrtcCompileProgram(" + generateProgVarName + ",0,nullptr);";
        m_interpreter.process(clingInput, &output);


        clingInput = R"RawMarker( 
            if (result != NVRTC_SUCCESS) {
                std::cerr << nvrtcGetErrorString(result) << std::endl;
                size_t log_size;
                nvrtcGetProgramLogSize()RawMarker" + generateProgVarName + R"RawMarker(, &log_size);
                char* log = new char[log_size];
                nvrtcGetProgramLog()RawMarker" + generateProgVarName + R"RawMarker(, log);
                std::cerr  << log << std::endl; 
                delete[] log;
                return -1;
            }
        )RawMarker";
        m_interpreter.process(clingInput, &output);

        clingInput = "nvrtcGetPTXSize(" + generateProgVarName + ",&ptxSize);";
        m_interpreter.process(clingInput, &output);
        
        m_interpreter.declare("char* ptx = new char[ptxSize];");

        clingInput = "nvrtcGetPTX(" + generateProgVarName + ",ptx);";
        m_interpreter.process(clingInput, &output);

        //TEST
        clingInput = "std::cout << ptx;";
        m_interpreter.process(clingInput, &output);


        return SUCCESS;
    } 



}