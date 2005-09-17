
#include <iostream>
#include <sstream>
#include <list>
#include <string>

int main(int, char**)
{
  int code;
  std::string seq;
  std::string message;
  std::istringstream s(std::string("100 seq12 Ma réponse"));

  s >> code >> seq;
  getline(s, message);
  std::cout << "Code: " << code << std::endl;
  std::cout << "Seq: " << seq << std::endl;
  std::cout << "Message: " << message << std::endl;
  return 0;
}

