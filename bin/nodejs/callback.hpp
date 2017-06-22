v8::Persistent<v8::Function> accountsChangedCb;
v8::Persistent<v8::Function> registrationStateChangedCb;

static void setAccountsChangedCb(const SwigV8Arguments &args) {
	SWIGV8_HANDLESCOPE();
	if (args[0]->IsObject()) {
			if (args[0]->IsFunction()){
			v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(args[0]);
			accountsChangedCb.Reset(v8::Isolate::GetCurrent(), func);
		} else {
			accountsChangedCb.Reset();
		}
	}
}


void accountsChanged(){
	SWIGV8_HANDLESCOPE();

	v8::Local<v8::Function> func = v8::Local<v8::Function>::New(v8::Isolate::GetCurrent(), accountsChangedCb);
	if (!func.IsEmpty()) {
		printf("accountsChanged Called | C++\n" );
		v8::Local<v8::Value> callback_args[] = { };
		v8::Handle<v8::Value> js_result = func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 0, callback_args);
	}
}


static void setRegistrationStateChangedCb(const SwigV8Arguments &args) {
	SWIGV8_HANDLESCOPE();

	if (args[0]->IsObject()) {
		if (args[0]->IsFunction()){
			v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(args[0]);
			registrationStateChangedCb.Reset(v8::Isolate::GetCurrent(), func);
		} else {
			registrationStateChangedCb.Reset();
		}
	}
}

void registrationStateChanged(const std::string& account_id,const std::string& state,int code,const std::string& detail_str){
	SWIGV8_HANDLESCOPE();

	v8::Local<v8::Function> func = v8::Local<v8::Function>::New(v8::Isolate::GetCurrent(), registrationStateChangedCb);
	if (!func.IsEmpty()) {
		printf("registrationStateChanged Called | C++\n" );
		v8::Local<v8::Value> callback_args[] = { SWIGV8_STRING_NEW(account_id.c_str()),SWIGV8_STRING_NEW(state.c_str()),
												 SWIGV8_INTEGER_NEW(code),SWIGV8_STRING_NEW(detail_str.c_str()) };

		v8::Handle<v8::Value> js_result = func->Call(SWIGV8_CURRENT_CONTEXT()->Global(), 4, callback_args);
	}
}
void init2(const SwigV8Arguments &args) {
	SWIGV8_HANDLESCOPE();

	using namespace std::placeholders;

	using std::bind;
	using DRing::exportable_callback;
	using DRing::ConfigurationSignal;
	using SharedCallback = std::shared_ptr<DRing::CallbackWrapperBase>;

	const std::map<std::string, SharedCallback> configEvHandlers = {
		exportable_callback<ConfigurationSignal::AccountsChanged>(bind(&accountsChanged)),
		exportable_callback<ConfigurationSignal::RegistrationStateChanged>(bind(&registrationStateChanged, _1, _2, _3, _4))
	};

	if (!DRing::init(static_cast<DRing::InitFlag>(DRing::DRING_FLAG_DEBUG)))
		return;

	registerConfHandlers(configEvHandlers);

	DRing::start();
}