
#include <iostream>
#include <sstream>
#include <list>
#include <string>

int main(int, char**)
{
  std::istringstream s(std::string("100 seq12 Maréponse \"sldkfjdfj s;dlfk\""));
  
  std::string output;
  std::list< std::string > args;
  while(s.good()) {
    s >> output;
    args.push_back(output);
  }

  for(std::list< std::string >::iterator pos = args.begin();
      pos != args.end();
      pos++) {
    std::cout << *pos << std::endl;
  }
}

