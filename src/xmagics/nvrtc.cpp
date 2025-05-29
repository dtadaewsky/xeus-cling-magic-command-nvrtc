/****************************************************************************************
* Copyright (c) 2025, David Tadaewsky                                                   *
* Copyright (c) 2025, FernUniversität in Hagen, Fakultät für Mathematik und Informatik  *
*                                                                                       *
* Distributed under the terms of the BSD 3-Clause License.                              *
*                                                                                       *
* The full license is in the file LICENSE, distributed with this software.              *
****************************************************************************************/

#include "nvrtc.hpp"

#include <fstream>
#include "cling/Interpreter/Value.h"
#include "../xmime_internal.hpp"

#define ERROR_CODE -1
#define SUCCESS 0

namespace xcpp
{
    void nvrtc::operator()(const std::string& line, const std::string& cell)
    {
        generateNVRTC(line,"\n\n" + cell);  // get new lines, for error response in correct line
    }

    void nvrtc::generateNVRTC(const std::string& line, const std::string& cell)
    {
        getCompileOptions(line);    //get input from magic command line

        foundHeaders.clear();   //reset header content
        foundContent.clear();   //reset header content
        getIncludePaths(cell);  //extract and load headerfile from magic command cell

        if(!initializationDone) //only at the first attempt 
        {   
            if(SUCCESS!=loadLibrarys()) return;     //load libs
            
            if(SUCCESS!=loadIncludes(getCudaIncludePath(line))) return; //load header with path
            
            if(SUCCESS!=defineCUDACheckError()) return; //define CUDA function for error response
           
            if(SUCCESS!=declareNVRTCVar())  return; //define vars
           
            if(SUCCESS!=initDevice())  return;    //init CUDA devices

            initializationDone=true;  //set var for init done
        }   
        if(printDeviceInfo) // if line has parameter for print device info then print infos
        {
            if(SUCCESS!=getDeviceInfo()) return;
        } 
        if(SUCCESS!=printDeviceName()) return;

    
        definePTX(cell);            //create PTX code
        generateKernelFunction();   //load function in modules
    }

    int nvrtc::getCompileOptions(const std::string& line)
    {
        //serach for GPU Info key word
        std::regex GPUInfo(R"(-GPUInfo(\s|$))");   
        if (std::regex_search(line, GPUInfo)) {
            printDeviceInfo=true;
        } else {
            printDeviceInfo=false;
        }
        //serach for -co and extract following entry
        std::regex pattern(R"(-co\s+((?:[^\s](?:[^\s]*))))"); 
        std::sregex_iterator it(line.begin(), line.end(), pattern);
        std::sregex_iterator end;

        compilerOptions.clear();    //clear befor set new entrys
        while (it != end) {
            compilerOptions.push_back((*it)[1]); 
            ++it;
        }
        return SUCCESS;
    } 

    int nvrtc::getIncludePaths(const std::string& content)
    {
        std::string tempContent;
        std::string cellWithoutComment=removeComments(content);     //exclude comment to not use comment include definitions

        std::regex pattern(R"#(#include\s*<([^>]+)>|#include\s*"([^"]+)")#");   //search for include instruction
        std::smatch match;


        std::string::const_iterator searchStart(cellWithoutComment.cbegin());
        while (std::regex_search(searchStart, cellWithoutComment.cend(), match, pattern)) {
            if (match[1].matched) {
                //if match allready found ignore this one
                if (std::find(foundHeaders.begin(), foundHeaders.end(), match[1].str()) == foundHeaders.end()) {
                    tempContent=readFileToString(match[1].str()); // read file
                    foundHeaders.push_back(match[1].str()); //register file in list
                    foundContent.push_back(tempContent);    //add content to list
                    getIncludePaths(tempContent);           //search also in file for include entrys
                } 
            } else if (match[2].matched) {
                //same methode like match 1 but fpr the "" notation
                if (std::find(foundHeaders.begin(), foundHeaders.end(), match[2].str()) == foundHeaders.end()) {
                    tempContent=readFileToString(match[2].str());
                    foundHeaders.push_back(match[2].str());
                    foundContent.push_back(tempContent);
                    getIncludePaths(tempContent);
                } 
            } 
            searchStart = match.suffix().first; //set for next match
        }
        return SUCCESS;
    } 

    int nvrtc::loadLibrarys()
    {
        
        std::string nvrtcLib = "libnvrtc.so";
        std::string cudaLib = "cuda.so";
        // Load libraries in cling 
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

    int nvrtc::loadIncludes(const std::string includePath)
    {
        //load headerfiles
        std::string nvrtcHeader = "/usr/local/cuda/include/nvrtc.h";    
        std::string cudaHeader = "/usr/local/cuda/include/cuda.h";
       
        if(includePath!="") //if include path was set from user use the set path else use the default values
        {
            nvrtcHeader = includePath + "nvrtc.h";
            cudaHeader = includePath + "cuda.h";
        } 
        //load header in cling
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
        /*
            define function in cling
            void checkCudaError(nvrtcResult result)
            { 
                if(result != NVRTC_SUCCESS) 
                {
                    std::cerr << "NVRTC Error:" << nvrtcGetErrorString(result) << std::endl;
                    return -1; 
                }
            }
        */

        std::string clingInput="void checkCudaError(nvrtcResult result){ if(result != NVRTC_SUCCESS) {std::cerr <<\"NVRTC Error:\" << nvrtcGetErrorString(result) << std::endl;return -1; }}";
        cling::Value output; 
        //load iostream header
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

        /*
            define function in cling
            void checkCudaError(CUresult result)
            {
                if(result != CUDA_SUCCESS) 
                {
                    const char* errorStr = nullptr;
                    cuGetErrorString(result, &errorStr);
                    std::cerr << "CUDA Error:" << errorStr << std::endl;
                    return -1;
                }
            }
        */

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
        //devine result var
        if(m_interpreter.declare("nvrtcResult XCnvrtc_result;")!=cling::Interpreter::CompilationResult::kSuccess)
        {
            std::cerr << "Could not declare nvrtcResult" << std::endl;
            return ERROR_CODE;
        } 
        return SUCCESS;
    } 
    
    int nvrtc::initDevice()
    {
        cling::Value output;
        std::string clingInput= "cuInit(0);"; 
        m_interpreter.process(clingInput, &output);
        //get number of CUDA devices in cling context and return it to xeus context
        clingInput= "CUdevice XCnvrtc_deviceInfo; int XCnvrtc_CUDAdeviceCount = 0; checkCudaError(cuDeviceGetCount(&XCnvrtc_CUDAdeviceCount));XCnvrtc_CUDAdeviceCount;"; 
        m_interpreter.process(clingInput, &output);
        foundCUDADevices= output.getLL();
        //for each device create device context und module
        for (int i = 0; i < foundCUDADevices; i++)
        {
            if(m_interpreter.declare("CUdevice XCnvrtc_device"+ std::to_string(i) +";")!=cling::Interpreter::CompilationResult::kSuccess)
            {
                std::cerr << "Could not init device: " << std::to_string(i) <<std::endl;
                  return ERROR_CODE;
            }
            clingInput= "checkCudaError(cuDeviceGet(&XCnvrtc_device"+ std::to_string(i) + ", " + std::to_string(i) + "));"; 
            m_interpreter.process(clingInput, &output);
            m_interpreter.declare("CUcontext XCnvrtc_cuContext"+std::to_string(i)+";");
            clingInput = "checkCudaError(cuCtxCreate(&XCnvrtc_cuContext" + std::to_string(i) + ", 0, XCnvrtc_device" + std::to_string(i) + "));";
            m_interpreter.process(clingInput, &output);
            m_interpreter.declare("CUmodule XCnvrtc_cuModule"+ std::to_string(i) + ";");
        } 
        return SUCCESS;
    }

    int nvrtc::getDeviceInfo()
    {

        //for each device get infos and print them, the complete function is executed in cling context
        cling::Value output;
        std::string clingInput= "std::cout << \"Found CUDA capable devices: \"<< XCnvrtc_CUDAdeviceCount <<std::endl;"; 
        m_interpreter.process(clingInput, &output);
        clingInput = R"RawMarker( 

            int XCnvrtc_maxVersion = 0, XCnvrtc_maxVersionIndex = 0;
            int XCnvrtc_minVersion = 999, XCnvrtc_minVersionIndex = 999;
            
            for (int i = 0; i < XCnvrtc_CUDAdeviceCount; i++) {

                cuDeviceGet(&XCnvrtc_deviceInfo, i);
        
                char XCnvrtc_gpuName[256];
                cuDeviceGetName(XCnvrtc_gpuName, 256, XCnvrtc_deviceInfo);
        
                int XCnvrtc_version, XCnvrtc_versionIndex;
                cuDeviceGetAttribute(&XCnvrtc_version, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, XCnvrtc_deviceInfo);
                cuDeviceGetAttribute(&XCnvrtc_versionIndex, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, XCnvrtc_deviceInfo);
        
                std::cout << "GPU Number:" << i << ": " << XCnvrtc_gpuName << std::endl;
                std::cout << "Compute Capability: " << XCnvrtc_version << "." << XCnvrtc_versionIndex << std::endl;
                std::cout << "Recommended NVCC-Architecture: -arch=sm_" << XCnvrtc_version  <<  XCnvrtc_versionIndex << std::endl;
        
                
                if (XCnvrtc_version > XCnvrtc_maxVersion || (XCnvrtc_version == XCnvrtc_maxVersion && XCnvrtc_versionIndex > XCnvrtc_maxVersionIndex)) {
                    XCnvrtc_maxVersion = XCnvrtc_version;
                    XCnvrtc_maxVersionIndex = XCnvrtc_versionIndex;
                }
        
                if (XCnvrtc_version < XCnvrtc_minVersion || (XCnvrtc_version == XCnvrtc_minVersion && XCnvrtc_versionIndex < XCnvrtc_minVersionIndex)) {
                    XCnvrtc_minVersion = XCnvrtc_version;
                    XCnvrtc_minVersionIndex = XCnvrtc_versionIndex;
                }
            }
            if(XCnvrtc_CUDAdeviceCount>1)
            {
                std::cout << "Max. Compute Capability: " << XCnvrtc_maxVersion << "." << XCnvrtc_maxVersionIndex << std::endl;
                std::cout << "Min. Compute Capability: " << XCnvrtc_minVersion << "." << XCnvrtc_minVersionIndex << std::endl;
            }


            )RawMarker";
            if(m_interpreter.process(clingInput, &output)!=cling::Interpreter::CompilationResult::kSuccess)
            {
                return ERROR_CODE;
            } 
        return SUCCESS;   
    }

    int nvrtc::definePTX(const std::string& code)
    {
        cling::Value output;
        std::string clingInput;
        std::string clingInputBackup;
        std::string clingInputIncludeNames;
        std::string clingInputIncludeContent;
        std::string generateProgVarName = "XCnvrtc_prog"; 
        int headerIndex=0;
        index++;

        generateProgVarName += std::to_string(index); //add index to programm var name, making repeated execution possible
        
        std::string declareInput = "nvrtcProgram " +generateProgVarName + ";"; 
        m_interpreter.declare(declareInput);
        std::string declarePTXSizeInput = "size_t XCnvrtc_ptxSize" + std::to_string(index); //also add index
        declarePTXSizeInput += ";";
        m_interpreter.declare(declarePTXSizeInput);


        clingInput= "const char *XCnvrtc_kernelCodeInCharArrey"+ std::to_string(index);
        clingInput += "=R\"RawMarker(" + code + ")RawMarker\";";    //add string values from cell in string var in cling 
        m_interpreter.process(clingInput, &output);

        //create the nvrtcCreateProgramm function line
        clingInput = "checkCudaError(nvrtcCreateProgram(&" + generateProgVarName; 
        clingInput += ",XCnvrtc_kernelCodeInCharArrey"+ std::to_string(index);

        if(foundHeaders.size()==0)
        {
            clingInput += ",\"xeus_cling.cu\",0,nullptr,nullptr));";    // finisch the command with nullptr when no header is used
        } 
        else
        {
            // if header files in use, then create const char* header names array
            clingInputBackup = "const char* XCnvrtc_header_names[] = {";
            
            for (const auto& header : foundHeaders) { //add headernames to header names array for each header entry
                clingInputIncludeNames  ="const char* XCnvrtc_header"+std::to_string(headerIndex) +"_name =\"" + header + "\";";    //header name

                m_interpreter.process(clingInputIncludeNames, &output); //cling input

                clingInputBackup += "XCnvrtc_header"+ std::to_string(headerIndex) +"_name,";    //add to array
                headerIndex++;
            }

            clingInputBackup += "};";
            m_interpreter.process(clingInputBackup, &output); //put array to cling


            headerIndex=0;
            //same procedure like the header name but with header content instead of names
            clingInputBackup = "const char* XCnvrtc_headers[] = {";
            for (const auto& content : foundContent) {
                  clingInputIncludeContent  ="const char* XCnvrtc_header"+std::to_string(headerIndex) +"_content = R\"RawMarker(\n" + content + ")RawMarker\";";
                  m_interpreter.process(clingInputIncludeContent, &output);
  
                  clingInputBackup += "XCnvrtc_header"+ std::to_string(headerIndex) +"_content,";
                  headerIndex++;
              }
              clingInputBackup += "};";
              m_interpreter.process(clingInputBackup, &output);


            clingInput += ",\"xeus_cling.cu\","+ std::to_string(foundHeaders.size()) +",XCnvrtc_headers,XCnvrtc_header_names));";
        } 
        
        m_interpreter.process(clingInput, &output); //insert nvrtcCreateProgram instruction to cling
        

        //Add compiler options
        if(compilerOptions.size()==0) clingInput = "XCnvrtc_result =nvrtcCompileProgram(" + generateProgVarName + ",0,nullptr);"; // no compiler options execute with nullptr
        else
        {
            //create array with options
            std::string options = "const char* XCnvrtc_options[] = {\n";    

            for (const std::string& sopt : compilerOptions) {//add options to array
                options += "\"" + sopt + "\",";
            }
            options += "};";
            m_interpreter.process(options, &output);

            clingInput = "XCnvrtc_result =nvrtcCompileProgram(" + generateProgVarName + "," + std::to_string(compilerOptions.size()) +",XCnvrtc_options);"; //execute with options 
        }  
    
        m_interpreter.process(clingInput, &output); //insert compile instruction to cling

        //print errors if compilation is not without errors or warnings
        clingInput = R"RawMarker( 
            if (XCnvrtc_result != NVRTC_SUCCESS) {
                std::cerr << nvrtcGetErrorString(XCnvrtc_result) << std::endl;
                size_t XCnvrtc_log_size;
                nvrtcGetProgramLogSize()RawMarker" + generateProgVarName + R"RawMarker(, &XCnvrtc_log_size);
                char* XCnvrtc_log = new char[XCnvrtc_log_size];
                nvrtcGetProgramLog()RawMarker" + generateProgVarName + R"RawMarker(, XCnvrtc_log);
                std::cerr  << XCnvrtc_log << std::endl; 
                delete[] XCnvrtc_log;
                return -1;
            }
        )RawMarker";
        m_interpreter.process(clingInput, &output);

        // generate PTX Code 
        clingInput = "checkCudaError(nvrtcGetPTXSize(" + generateProgVarName + ",&XCnvrtc_ptxSize" + std::to_string(index) + "));";
        m_interpreter.process(clingInput, &output);
        m_interpreter.declare("char* XCnvrtc_ptx" + std::to_string(index) + " = new char[XCnvrtc_ptxSize" + std::to_string(index) + "];");
        clingInput = "checkCudaError(nvrtcGetPTX(" + generateProgVarName + ",XCnvrtc_ptx" + std::to_string(index) + "));";
        m_interpreter.process(clingInput, &output);
        // get PTX Code and get the output from cling
        clingInput = "std::string ptxString(XCnvrtc_ptx" + std::to_string(index) + ", XCnvrtc_ptxSize"+ std::to_string(index) + ");";
        m_interpreter.process(clingInput, &output);
        nl::json pub_data = mime_repr(output);  //output from cling as string data

        listOfNames.clear();                    //clear data before rerun                                                                
        listOfNames = extractFunctionNames(pub_data["text/plain"].get<std::string>());  //search for function in PTX Code
 
        return SUCCESS;
    } 

    int nvrtc::generateKernelFunction()
    { 
        cling::Value output;
        std::string clingInput;

        //for each GPU generate cotext and module with PTX
        for (int i = 0; i < foundCUDADevices; i++)
        {
            clingInput = "cuCtxSetCurrent(XCnvrtc_cuContext"+ std::to_string(i) + ");";
            m_interpreter.process(clingInput, &output);
            clingInput = "cuModuleLoadData(&XCnvrtc_cuModule" + std::to_string(i) + ", XCnvrtc_ptx" + std::to_string(index) + ");";
            m_interpreter.process(clingInput, &output);
        }
        // for each function create a function 
        for (std::string s: listOfNames)
        { 
            // if the function name is allready registered then the registration if the CU funciton name is not nessesary and only print the found function
            if (std::find(registeredFunctionNames.begin(), registeredFunctionNames.end(), s) == registeredFunctionNames.end())
            {
                for (int i = 0; i < foundCUDADevices; i++)
                {
                    if(foundCUDADevices==1) 
                    {
                        clingInput = "CUfunction "+ demangle(s) +";";
                        m_interpreter.declare(clingInput);
                        std::cout << demangle(s) << std::endl;
                    } 
                    else //if more then one GPU then add index _GPU + number
                    {
                        clingInput = "CUfunction "+ demangle(s) +"_GPU"+ std::to_string(i) + ";";
                        m_interpreter.declare(clingInput);
                        std::cout << demangle(s) << "_GPU" << std::to_string(i)<< std::endl;
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
                        std::cout << demangle(s) << std::endl;
                    } 
                    else
                    {
                        std::cout << demangle(s) << "_GPU" << std::to_string(i)<< std::endl;
                    } 
                } 
            }  

            // load functions in the module for each GPU
            for (int i = 0; i < foundCUDADevices; i++)
            {
                if(foundCUDADevices==1)
                {
                    clingInput = R"RawMarker(checkCudaError(cuModuleGetFunction(&)RawMarker"+ demangle(s) + R"RawMarker(, XCnvrtc_cuModule0, ")RawMarker"+ s +"\"));";
                    m_interpreter.process(clingInput, &output);
                }
                else // use function names with GPU index when multiple GPUs are used
                {
                    clingInput = "cuCtxSetCurrent(XCnvrtc_cuContext"+ std::to_string(i) + ");";
                    m_interpreter.process(clingInput, &output);
                    clingInput = R"RawMarker(checkCudaError(cuModuleGetFunction(&)RawMarker"+ demangle(s) +"_GPU"+ std::to_string(i) + R"RawMarker(, XCnvrtc_cuModule)RawMarker" + std::to_string(i)+ R"RawMarker(, ")RawMarker"+ s + "\"));";
                    m_interpreter.process(clingInput, &output);
                } 
            } 
            clingInput = "cuCtxSetCurrent(XCnvrtc_cuContext0);";
            m_interpreter.process(clingInput, &output);
        } 

        return SUCCESS;
    } 

    std::list<std::string> nvrtc::extractFunctionNames(const std::string& ptx)
    {
        std::list<std::string> listOfFunctions;
        std::string singleFinding;
        std::regex ptxFunctionName(R"(\/\/ .globl\s(\w*))"); // seraching for global function definition
        auto words_begin =std::sregex_iterator(ptx.begin(),ptx.end(),ptxFunctionName);
        auto words_end =std::sregex_iterator();

        for(std::sregex_iterator i = words_begin; i != words_end; ++i){
            std::smatch match = *i;
            listOfFunctions.push_back(match[1].str());  // list all function names
        }
        return listOfFunctions;
    }

    int nvrtc::printDeviceName()
    {
        std::string clingInput;
        cling::Value output;
        if(foundCUDADevices>1) // if more the one device was found print name of GPU and variable of device and context
        {
            clingInput = 
                R"RawMarker(
                    for (int i = 0; i < XCnvrtc_CUDAdeviceCount; i++) {

                        cuDeviceGet(&XCnvrtc_deviceInfo, i);
                
                        char XCnvrtc_gpuName[256];
                        cuDeviceGetName(XCnvrtc_gpuName, 256, XCnvrtc_deviceInfo);
        
                        std::cout << "GPU: " << XCnvrtc_gpuName << " CUdevice: XCnvrtc_device"<< i << " CUcontext: XCnvrtc_cuContext"<< i <<std::endl;
                    } 
                )RawMarker";
            if(m_interpreter.process(clingInput, &output)!=cling::Interpreter::CompilationResult::kSuccess)
            {
                return ERROR_CODE;
            } 
            
        } 
        return SUCCESS;  
    } 

    std::string nvrtc::getCudaIncludePath(const std::string line)
    {
        std::regex pattern(R"(-cudaPath\s+((?:[^\s](?:[^\s]*))))");     //searching for -cudaPath and following part of string
        std::smatch match; 
        std::string path;
        if (std::regex_search(line,match, pattern)) {
            path = match[1].str();
            if(path.back()!='/') path += "/";
            if(path.front()!='/') path = "/" + path;
            return path;        //return path when -cudaPath was found in string
        } 
        return "";
    } 

    std::string nvrtc::readFileToString(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file) {
            throw std::runtime_error("Could not open File: " + filePath);
        }
    
        std::stringstream buffer;
        buffer << file.rdbuf();  // file in stringstream
        return buffer.str();
    }

    std::string nvrtc::removeComments(const std::string& code){
        //regex for single line comment
        std::regex singleLineCommentPattern("//.*");
        //regex for multi line comment
        std::regex multiLineCommentPattern("/\\*[\\s\\S]*?\\*/");
        //remove both pattern
        std::string withoutSLComment= std::regex_replace(code,singleLineCommentPattern,"");
        return std::regex_replace(withoutSLComment,multiLineCommentPattern,"");
    }
    
    std::string nvrtc::demangle(const std::string& mangled) {
        std::string result = mangled.substr(2); // remove _Z
        
        int lengthOfName=0,countNumbers =0;
        while (true)
        {
            if (std::isdigit(result[0])) 
            { 
                countNumbers++;
                result = result.substr(1);   //remove all numbers afer _Z
            }
            else break;
        } 
        try 
        {
            lengthOfName = std::stoi(mangled.substr(2,countNumbers));
        } 
        catch (const std::invalid_argument& e) {
            std::cerr << "no number!" << std::endl;
        } 
        catch (const std::out_of_range& e) {
            std::cerr << "out of range" << std::endl;
        }
        return mangled.substr(countNumbers +2 , lengthOfName) +"__"+ mangled.substr(countNumbers +2+lengthOfName); //return a short version of mangled function name
    }
}
