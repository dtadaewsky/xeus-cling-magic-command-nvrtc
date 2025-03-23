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

#include "../xmime_internal.hpp"



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
           
            if(SUCCESS!=initDevice(false))  return;  //TODO Kopfzeilenparameter von Zelle nutzen um diese Option freizuschalten

            initializationDone=true;    
        }   


        std::string clingInput;
        cling::Value output;
        for (int i = 0; i < foundCUDADevices; i++)
        {
            clingInput = 
            R"RawMarker(
                for (int i = 0; i < CUDAdeviceCount; i++) {

                    cuDeviceGet(&deviceInfo, i);
            
                    char gpuName[256];
                    cuDeviceGetName(gpuName, 256, deviceInfo);
    
                    std::cout << "GPU: " << gpuName << "    CUdevice VarName: device"<< i << std::endl;
                } 
            )RawMarker";
            m_interpreter.process(clingInput, &output);
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
        std::string clingInput="void checkCudaError(nvrtcResult result){ if(result != NVRTC_SUCCESS) {std::cerr <<\"NVRTC Error:\" << nvrtcGetErrorString(result) << std::endl;return -1; }}";
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

        clingInput="void checkCudaError(CUresult result){ if(result != CUDA_SUCCESS) {const char* errorStr = nullptr; cuGetErrorString(result, &errorStr); std::cerr <<\"CUDA Error:\" << errorStr << std::endl;return -1; }}";
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
        
        generateProgVarName += std::to_string(index); //TODO  Sollte immer einen neuen namen haben, vielleicht auch mit index aus einer Variable
        
        std::string declareInput = "nvrtcProgram " +generateProgVarName + ";"; 
        m_interpreter.declare(declareInput);
        std::string declarePTXSizeInput = "size_t ptxSize" + std::to_string(index); 
        declarePTXSizeInput += ";";//TODO  Sollte immer einen neuen namen haben, vielleicht auch mit index aus einer Variable
        m_interpreter.declare(declarePTXSizeInput);


        clingInput= "const char *kernelCodeInCharArrey"+ std::to_string(index);
        clingInput += "=R\"RawMarker(" + code + ")RawMarker\";";
        m_interpreter.process(clingInput, &output);


        clingInput = "checkCudaError(nvrtcCreateProgram(&" + generateProgVarName; 
        clingInput += ", kernelCodeInCharArrey"+ std::to_string(index);
        clingInput += ",\"xeus_cling.cu\",0,nullptr,nullptr));";
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

        clingInput = "checkCudaError(nvrtcGetPTXSize(" + generateProgVarName + ",&ptxSize" + std::to_string(index) + "));";
        m_interpreter.process(clingInput, &output);
        
        m_interpreter.declare("char* ptx" + std::to_string(index) + " = new char[ptxSize" + std::to_string(index) + "];");

        clingInput = "checkCudaError(nvrtcGetPTX(" + generateProgVarName + ",ptx" + std::to_string(index) + "));";
        m_interpreter.process(clingInput, &output);

        //TEST
        clingInput = "std::string ptxString(ptx" + std::to_string(index) + ", ptxSize"+ std::to_string(index) + ");";
        m_interpreter.process(clingInput, &output);
        nl::json pub_data = mime_repr(output);

        std::list<std::string> listOfNames = extractFunctionNames(pub_data["text/plain"].get<std::string>());
 
        for (int i = 0; i < foundCUDADevices; i++)
        {
            clingInput = "cuModuleLoadData(&cuModule" + std::to_string(i) + ", ptx" + std::to_string(index) + ");";
            m_interpreter.process(clingInput, &output);
        }

        for (std::string s: listOfNames)
        { 
            if (std::find(registeredFunctionNames.begin(), registeredFunctionNames.end(), s) == registeredFunctionNames.end())
            {
                for (int i = 0; i < foundCUDADevices; i++)
                {
                    if(foundCUDADevices==1)
                    {
                        clingInput = "CUfunction "+ s +";";
                        m_interpreter.declare(clingInput);
                        std::cout << s << std::endl;
                    } 
                    else
                    {
                        clingInput = "CUfunction "+ s +"_GPU"+ std::to_string(i) + ";";
                        m_interpreter.declare(clingInput);
                        std::cout << s << "_GPU" << std::to_string(i)<< std::endl;
                    } 
                } 
                registeredFunctionNames.push_back(s);  
            }
            else
            {
                for (int i = 0; i < foundCUDADevices; i++)
                {
                    if(foundCUDADevices==1)
                    {
                        std::cout << s << std::endl;
                    } 
                    else
                    {
                        std::cout << s << "_GPU" << std::to_string(i)<< std::endl;
                    } 
                } 
            }  


            for (int i = 0; i < foundCUDADevices; i++)
            {
                if(foundCUDADevices==1)
                {
                    clingInput = R"RawMarker(checkCudaError(cuModuleGetFunction(&)RawMarker"+ s + R"RawMarker(, cuModule0, ")RawMarker"+ s +"\"));";
                    m_interpreter.process(clingInput, &output);
                }
                else
                {
                    clingInput = R"RawMarker(checkCudaError(cuModuleGetFunction(&)RawMarker"+ s +"_GPU"+ std::to_string(i) + R"RawMarker(, cuModule)RawMarker" + std::to_string(i)+ R"RawMarker(, ")RawMarker"+ s + "\"));";
                    m_interpreter.process(clingInput, &output);
                } 
            } 
        } 

        index++;

        return SUCCESS;
    } 

    int nvrtc::initDevice(bool getDeviceInfo)
    {
        cling::Value output;
        std::string clingInput= "cuInit(0);"; 
        m_interpreter.process(clingInput, &output);

        clingInput= "CUdevice deviceInfo; int CUDAdeviceCount = 0; checkCudaError(cuDeviceGetCount(&CUDAdeviceCount));CUDAdeviceCount;"; 
        m_interpreter.process(clingInput, &output);
        foundCUDADevices= output.getLL();

        ///foundCUDADevices=2; //TODO TEST!!!

        for (int i = 0; i < foundCUDADevices; i++)
        {
            if(m_interpreter.declare("CUdevice device"+ std::to_string(i) +";")!=cling::Interpreter::CompilationResult::kSuccess)
            {
                std::cerr << "Could not init device: " << std::to_string(i) <<std::endl;
                //  return ERROR_CODE;
            }
           // std::cout << "device: " << std::to_string(i) << std::endl;
            clingInput= "checkCudaError(cuDeviceGet(&device"+ std::to_string(i) + ", " + std::to_string(i) + "));"; 
            m_interpreter.process(clingInput, &output);
            m_interpreter.declare("CUcontext cuContext"+std::to_string(i)+";");
            clingInput = "checkCudaError(cuCtxCreate(&cuContext" + std::to_string(i) + ", 0, device" + std::to_string(i) + "));";
            m_interpreter.process(clingInput, &output);
            m_interpreter.declare("CUmodule cuModule"+ std::to_string(i) + ";");
        } 
        

        
        if(getDeviceInfo)
        {
            clingInput= "std::cout << \"Found CUDA capable devices: \"<< CUDAdeviceCount <<std::endl;"; 
            m_interpreter.process(clingInput, &output);
            clingInput = R"RawMarker( 

            int maxVersion = 0, maxVersionIndex = 0;
            int minVersion = 999, minVersionIndex = 999;
            
            for (int i = 0; i < CUDAdeviceCount; i++) {

                cuDeviceGet(&deviceInfo, i);
        
                char gpuName[256];
                cuDeviceGetName(gpuName, 256, deviceInfo);
        
                int version, versionIndex;
                cuDeviceGetAttribute(&version, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, deviceInfo);
                cuDeviceGetAttribute(&versionIndex, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, deviceInfo);
        
                std::cout << "GPU Number:" << i << ": " << gpuName << std::endl;
                std::cout << "Compute Capability: " << version << "." << versionIndex << std::endl;
                std::cout << "Recommended NVCC-Architecture: -arch=sm_" << version  <<  versionIndex << std::endl;
        
                
                if (version > maxVersion || (version == maxVersion && versionIndex > maxVersionIndex)) {
                    maxVersion = version;
                    maxVersionIndex = versionIndex;
                }
        
                if (version < minVersion || (version == minVersion && versionIndex < minVersionIndex)) {
                    minVersion = version;
                    minVersionIndex = versionIndex;
                }
            }
            if(CUDAdeviceCount>1)
            {
                std::cout << "Max. Compute Capability: " << maxVersion << "." << maxVersionIndex << std::endl;
                std::cout << "Min. Compute Capability: " << minVersion << "." << minVersionIndex << std::endl;
            }


            )RawMarker";
            m_interpreter.process(clingInput, &output);
        }     

        

       // if(foundCUDADevices==1)
      //  {
       //     clingInput= "cuDeviceGet(&device0, 0);"; 
       //     m_interpreter.process(clingInput, &output);
       //     m_interpreter.declare("CUcontext cuContext0;");
        //    clingInput = "checkCudaError(cuCtxCreate(&cuContext0, 0, device0));";
       //     m_interpreter.process(clingInput, &output);
       //     m_interpreter.declare("CUmodule cuModule0;");
     //   } 
       // else if(foundCUDADevices>1)
        //{
            
       // } 




        return 0;
    } 

    std::list<std::string> nvrtc::extractFunctionNames(const std::string& ptx)
    {
        std::list<std::string> listOfFunctions;
        std::string singleFinding;
        std::regex ptxFunctionName(R"(\/\/ .globl\s(\w*))");
        auto words_begin =std::sregex_iterator(ptx.begin(),ptx.end(),ptxFunctionName);
        auto words_end =std::sregex_iterator();

        for(std::sregex_iterator i = words_begin; i != words_end; ++i){
            std::smatch match = *i;
            listOfFunctions.push_back(match[1].str());
        }
        return listOfFunctions;
    }
}