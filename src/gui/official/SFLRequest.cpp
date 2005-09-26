class AnswerManagerImpl
{
public:
  void setPhoneLineManager(PhoneLineManager *manager);
  
private:
  PhoneLineManager *mManager;
};


void
CallRelatedRequest::onError(Call call, const std::string &code, const std::string &message)
{
  PhoneLineManager::instance().error();
}

void
CallRelatedRequest::onEntry(Call, const std::string &, const std::string &)
{}

void
CallRelatedRequest::onSuccess(Call, const std::string &, const std::string &)
{}
