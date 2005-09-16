#include <stdexcept>
#include <string>
#include <iostream>
#include <map>


class Request
{
public:
  Request()
  {
    std::cout << "This is a Normal Request" << std::endl;
  }
};

class SpecificRequest : public Request
{
public:
  SpecificRequest()
  {
    std::cout << "This is a Specific Request" << std::endl;
  }
};


class RequestCreatorBase
{
public:
  virtual Request *create() = 0;
  virtual RequestCreatorBase *clone() = 0;
};

template< typename T >
class RequestCreator : public RequestCreatorBase
{
public:
  virtual Request *create()
  {
    return new T();
  }

  virtual RequestCreatorBase *clone()
  {
    return new RequestCreator< T >();
  }
};


class RequestFactory
{
public:
  Request *create(const std::string &requestname)
  {
    std::map< std::string, RequestCreatorBase * >::iterator pos = mRequests.find(requestname);
    if(pos == mRequests.end()) {
      throw std::runtime_error("there's no request of that name");
    }
    
    return pos->second->create();
  }

  template< typename T >
  void registerRequest(const std::string &requestname)
  {
    std::map< std::string, RequestCreatorBase * >::iterator pos = 
      mRequests.find(requestname);
    if(pos != mRequests.end()) {
      delete pos->second;
      mRequests.erase(pos);
    }
    
    mRequests.insert(std::make_pair(requestname, new RequestCreator< T >()));
  }

 private:
  std::map< std::string, RequestCreatorBase * > mRequests;
};


int main(int, char **) 
{
  RequestFactory factory;

  factory.registerRequest< Request > ("requestsimple");
  factory.registerRequest< SpecificRequest >("requestspecific");
  
  std::cout << "First one" << std::endl;
  delete factory.create("requestsimple");
  
  std::cout << "Second one" << std::endl;
  delete factory.create("requestspecific");

  factory.registerRequest< SpecificRequest >("requestsimple");
  std::cout << "Third one" << std::endl;
  delete factory.create("requestsimple");

  return 0;
}
