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




namespace xcpp
{
    void nvrtc::operator()(const std::string& line, const std::string& cell)
    {
        generateNVRTC(line,cell);
    }

    void nvrtc::generateNVRTC(const std::string& line, const std::string& cell)
    {
        cling::Value output;
       
          loadLibrarys();
          std::cout << "Load Library"<< std::endl;                  //Debug Info
          loadIncludes();
          std::cout << "Load Includes"<< std::endl;                 //Debug Info

    
        
    }

    int nvrtc::loadLibrarys(){

        m_interpreter.loadLibrary("libnvrtc.so",true);

        m_interpreter.loadLibrary("cuda.so",true);
        return 0;
    }

    int nvrtc::loadIncludes(){

        m_interpreter.loadHeader("/usr/local/cuda/include/nvrtc.h");    //TODO hier vielleicht ein Automatische suche nach den Daten
        m_interpreter.loadHeader("/usr/local/cuda/include/cuda.h");
        return 0;
    }



}