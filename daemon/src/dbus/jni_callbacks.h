



extern struct callmanager_callback wrapper_callback_struct;
void on_new_call_created_wrapper (const std::string& accountID,
                                         const std::string& callID,
                                         const std::string& to);
void on_call_state_changed_wrapper(const std::string& callID,
                                          const std::string& to);
void on_incoming_call_wrapper (const std::string& accountID,
                                      const std::string& callID,
                                      const std::string& from);
void on_transfer_state_changed_wrapper (const std::string& result);

void on_conference_created_wrapper (const std::string& confID);
void on_conference_removed_wrapper (const std::string& confID);
void on_conference_state_changed_wrapper(const std::string& confID,const std::string& state);
void on_incoming_message_wrapper(const std::string& ID, const std::string& from, const std::string& msg);

extern struct configurationmanager_callback wrapper_configurationcallback_struct;
extern void on_accounts_changed_wrapper ();
extern void on_account_state_changed_wrapper (const std::string& accoundID, int const& state);
extern void on_account_state_changed_with_code_wrapper (const std::string& accoundID, const std::string& state, const int32_t& code);
