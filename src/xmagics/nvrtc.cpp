#include "nvrtc.hpp"



namespace xcpp
{
    
    void nvrtc::operator()(const std::string& line, const std::string& cell)
    {
        std::cerr << line;

        std::cout << cell;
        //TODO find keyWord -keepFiles -set CudaFiles??
        //std::cerr << "xx"<< cell << "xx"<< std::endl;



      //  generateFunction(cell);
        //generateNVRTC(cell);
    }
}