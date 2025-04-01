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
        generateNVRTC(line,cell);
    }

    void nvrtc::generateNVRTC(const std::string& line, const std::string& cell)
    {
        getCompileOptions(line);

        foundHeaders.clear();
        foundContent.clear();
        getIncludePaths(cell);

        if(!initializationDone)
        {   
            if(SUCCESS!=loadLibrarys()) return;
            
            if(SUCCESS!=loadIncludes(getCudaIncludePath(line))) return;
            
            if(SUCCESS!=defineCUDACheckError()) return;
           
            if(SUCCESS!=declareNVRTCVar())  return;
           
            if(SUCCESS!=initDevice())  return;  //TODO Kopfzeilenparameter von Zelle nutzen um diese Option freizuschalten

            initializationDone=true;    
        }   
        if(printDeviceInfo)
        {
            if(SUCCESS!=getDeviceInfo()) return;
        } 
        if(SUCCESS!=printDeviceName()) return;

    
        definePTX(cell);
        generateKernelFunction();
    }

    int nvrtc::getCompileOptions(const std::string& line)
    {
        std::regex GPUInfo(R"(-GPUInfo(\s|$))");
        if (std::regex_search(line, GPUInfo)) {
            printDeviceInfo=true;
        } else {
            printDeviceInfo=false;
        }

        std::regex pattern(R"(-co\s+((?:[^\s](?:[^\s]*))))"); 
        std::sregex_iterator it(line.begin(), line.end(), pattern);
        std::sregex_iterator end;

        compilerOptions.clear();
        while (it != end) {
            compilerOptions.push_back((*it)[1]); 
            ++it;
        }
        return SUCCESS;
    } 

    int nvrtc::getIncludePaths(const std::string& content)
    {
        std::string tempContent;
        std::string cellWithoutComment=removeComments(content);     //exclude Commet to not use comment include definitions

        std::regex pattern(R"#(#include\s*<([^>]+)>|#include\s*"([^"]+)")#");
        std::smatch match;


        std::string::const_iterator searchStart(cellWithoutComment.cbegin());
        while (std::regex_search(searchStart, cellWithoutComment.cend(), match, pattern)) {
            if (match[1].matched) {
                //if match allready found ignore this one //TODO List of matches exist allready oder ??
                if (std::find(foundHeaders.begin(), foundHeaders.end(), match[1].str()) == foundHeaders.end()) {
                    tempContent=readFileToString(match[1].str());
                    foundHeaders.push_back(match[1].str());
                    foundContent.push_back(tempContent);
                    getIncludePaths(tempContent);
                } 
            } else if (match[2].matched) {
                if (std::find(foundHeaders.begin(), foundHeaders.end(), match[2].str()) == foundHeaders.end()) {
                    tempContent=readFileToString(match[2].str());
                    foundHeaders.push_back(match[2].str());
                    foundContent.push_back(tempContent);
                    getIncludePaths(tempContent);
                } 
            } 
            searchStart = match.suffix().first;
        }
        return SUCCESS;
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

    int nvrtc::loadIncludes(const std::string includePath)
    {
       
        std::string nvrtcHeader = "/usr/local/cuda/include/nvrtc.h";    //TODO hier vielleicht ein Automatische suche nach den Daten
        std::string cudaHeader = "/usr/local/cuda/include/cuda.h";
       
        if(includePath!="")
        {
            nvrtcHeader = includePath + "nvrtc.h";
            cudaHeader = includePath + "cuda.h";
        } 
        

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

        clingInput= "CUdevice XCnvrtc_deviceInfo; int XCnvrtc_CUDAdeviceCount = 0; checkCudaError(cuDeviceGetCount(&XCnvrtc_CUDAdeviceCount));XCnvrtc_CUDAdeviceCount;"; 
        m_interpreter.process(clingInput, &output);
        foundCUDADevices= output.getLL();

        for (int i = 0; i < foundCUDADevices; i++)
        {
            if(m_interpreter.declare("CUdevice XCnvrtc_device"+ std::to_string(i) +";")!=cling::Interpreter::CompilationResult::kSuccess)
            {
                std::cerr << "Could not init device: " << std::to_string(i) <<std::endl;
                //  return ERROR_CODE;
            }
           // std::cout << "device: " << std::to_string(i) << std::endl;
            clingInput= "checkCudaError(cuDeviceGet(&XCnvrtc_device"+ std::to_string(i) + ", " + std::to_string(i) + "));"; 
            m_interpreter.process(clingInput, &output);
            m_interpreter.declare("CUcontext XCnvrtc_cuContext"+std::to_string(i)+";");
            clingInput = "checkCudaError(cuCtxCreate(&XCnvrtc_cuContext" + std::to_string(i) + ", 0, XCnvrtc_device" + std::to_string(i) + "));";
            m_interpreter.process(clingInput, &output);
            m_interpreter.declare("CUmodule XCnvrtc_cuModule"+ std::to_string(i) + ";");
        } 
        return 0;
    }

    int nvrtc::getDeviceInfo()
    {
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

        generateProgVarName += std::to_string(index); //TODO  Sollte immer einen neuen namen haben, vielleicht auch mit index aus einer Variable
        
        std::string declareInput = "nvrtcProgram " +generateProgVarName + ";"; 
        m_interpreter.declare(declareInput);
        std::string declarePTXSizeInput = "size_t XCnvrtc_ptxSize" + std::to_string(index); 
        declarePTXSizeInput += ";";//TODO  Sollte immer einen neuen namen haben, vielleicht auch mit index aus einer Variable
        m_interpreter.declare(declarePTXSizeInput);


        clingInput= "const char *XCnvrtc_kernelCodeInCharArrey"+ std::to_string(index);
        clingInput += "=R\"RawMarker(" + code + ")RawMarker\";";
        m_interpreter.process(clingInput, &output);


        clingInput = "checkCudaError(nvrtcCreateProgram(&" + generateProgVarName; 
        clingInput += ",XCnvrtc_kernelCodeInCharArrey"+ std::to_string(index);

        if(foundHeaders.size()==0)
        {
            clingInput += ",\"xeus_cling.cu\",0,nullptr,nullptr));";
        } 
        else
        {

            clingInputBackup = "const char* XCnvrtc_header_names[] = {";
            
            for (const auto& header : foundHeaders) {
                clingInputIncludeNames  ="const char* XCnvrtc_header"+std::to_string(headerIndex) +"_name =\"" + header + "\";";

                m_interpreter.process(clingInputIncludeNames, &output);

                clingInputBackup += "XCnvrtc_header"+ std::to_string(headerIndex) +"_name,";
                headerIndex++;
            }

            clingInputBackup += "};";
            m_interpreter.process(clingInputBackup, &output);


            headerIndex=0;
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
        
        m_interpreter.process(clingInput, &output);
        
        if(compilerOptions.size()==0) clingInput = "XCnvrtc_result =nvrtcCompileProgram(" + generateProgVarName + ",0,nullptr);";
        else
        {
            std::string options = "const char* options[] = {\n";    

            for (const std::string& sopt : compilerOptions) {
                options += "\"" + sopt + "\",";
            }
            options += "};";
            m_interpreter.process(options, &output);

            clingInput = "XCnvrtc_result =nvrtcCompileProgram(" + generateProgVarName + "," + std::to_string(compilerOptions.size()) +",options);";
        }  
    
        m_interpreter.process(clingInput, &output);


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

        clingInput = "checkCudaError(nvrtcGetPTXSize(" + generateProgVarName + ",&XCnvrtc_ptxSize" + std::to_string(index) + "));";
        m_interpreter.process(clingInput, &output);
        
        m_interpreter.declare("char* XCnvrtc_ptx" + std::to_string(index) + " = new char[XCnvrtc_ptxSize" + std::to_string(index) + "];");

        clingInput = "checkCudaError(nvrtcGetPTX(" + generateProgVarName + ",XCnvrtc_ptx" + std::to_string(index) + "));";
        m_interpreter.process(clingInput, &output);

        //TEST
        clingInput = "std::string ptxString(XCnvrtc_ptx" + std::to_string(index) + ", XCnvrtc_ptxSize"+ std::to_string(index) + ");";
        m_interpreter.process(clingInput, &output);
        nl::json pub_data = mime_repr(output);

        listOfNames.clear();                                                                        //clear data from run before
        listOfNames = extractFunctionNames(pub_data["text/plain"].get<std::string>());
 
        return SUCCESS;
    } 

    int nvrtc::generateKernelFunction()
    { 
        cling::Value output;
        std::string clingInput;



        for (int i = 0; i < foundCUDADevices; i++)
        {
            clingInput = "cuModuleLoadData(&XCnvrtc_cuModule" + std::to_string(i) + ", XCnvrtc_ptx" + std::to_string(index) + ");";
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
                        clingInput = "CUfunction "+ demangle(s) +";";
                        m_interpreter.declare(clingInput);
                        std::cout << demangle(s) << std::endl;
                    } 
                    else
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


            for (int i = 0; i < foundCUDADevices; i++)
            {
                if(foundCUDADevices==1)
                {
                    clingInput = R"RawMarker(checkCudaError(cuModuleGetFunction(&)RawMarker"+ demangle(s) + R"RawMarker(, XCnvrtc_cuModule0, ")RawMarker"+ s +"\"));";
                    m_interpreter.process(clingInput, &output);
                }
                else
                {
                    clingInput = R"RawMarker(checkCudaError(cuModuleGetFunction(&)RawMarker"+ demangle(s) +"_GPU"+ std::to_string(i) + R"RawMarker(, XCnvrtc_cuModule)RawMarker" + std::to_string(i)+ R"RawMarker(, ")RawMarker"+ s + "\"));";
                    m_interpreter.process(clingInput, &output);
                } 
            } 
        } 

        return SUCCESS;
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

    int nvrtc::printDeviceName()
    {
        std::string clingInput;
        cling::Value output;
        if(foundCUDADevices>1)
        {
            clingInput = 
                R"RawMarker(
                    for (int i = 0; i < XCnvrtc_CUDAdeviceCount; i++) {

                        cuDeviceGet(&XCnvrtc_deviceInfo, i);
                
                        char XCnvrtc_gpuName[256];
                        cuDeviceGetName(XCnvrtc_gpuName, 256, XCnvrtc_deviceInfo);
        
                        std::cout << "GPU: " << XCnvrtc_gpuName << "    CUdevice VarName: XCnvrtc_device"<< i << std::endl;
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
        std::regex pattern(R"(-cudaPath\s+((?:[^\s](?:[^\s]*))))"); 
        std::smatch match; 
        std::string path;
        if (std::regex_search(line,match, pattern)) {
            path = match[1].str();
            if(path.back()!='/') path += "/";
            if(path.front()!='/') path = "/" + path;
            return path;
        } 
        return "";
    } 

    std::string nvrtc::readFileToString(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file) {
            throw std::runtime_error("Could not open File: " + filePath);
        }
    
        std::stringstream buffer;
        buffer << file.rdbuf();  // Datei in den Stringstream einlesen
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
        return mangled.substr(countNumbers +2 , lengthOfName) +"__"+ mangled.substr(countNumbers +2+lengthOfName);
    }
}