%header %{

namespace jami {
struct DataView {
  const uint8_t* data;
  size_t size;
};
struct Data {
    std::vector<uint8_t> data;
};
struct String {
    std::string str;
};

}

%}

namespace jami {
struct DataView;
struct Data;
struct String;

%typemap(jni) DataView "jbyteArray"
%typemap(jtype) DataView "byte[]"
%typemap(jstype) DataView "byte[]"

%typemap(jni) Data "jbyteArray"
%typemap(jtype) Data "byte[]"
%typemap(jstype) Data "byte[]"

%typemap(jni) String "jbyteArray"
%typemap(jtype) String "byte[]"
%typemap(jstype) String "byte[]"

%typemap(javadirectorin) Data "$jniinput"
%typemap(javadirectorin) String "$jniinput"
%typemap(javadirectorout) DataView "$javacall"

%typemap(in) Data
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
     return $null;
    }
    jsize len = jenv->GetArrayLength($input);
    $1.data.resize(len);
    jenv->GetByteArrayRegion($input, 0, len, (jbyte*)$1.data.data()); %}

%typemap(in) String
%{ if(!$input) {
     SWIG_JavaThrowException(jenv, SWIG_JavaNullPointerException, "null string");
     return $null;
    }
    jsize len = jenv->GetArrayLength($input);
    $1.str.resize(len);
    jenv->GetByteArrayRegion($input, 0, len, (jbyte*)$1.str.data()); %}

%typemap(out) DataView {
  jbyteArray r = jenv->NewByteArray($1.size);
  jenv->SetByteArrayRegion(r, 0, $1.size, (const jbyte *)$1.data);
  $result = r;
}

%typemap(javain) Data "$javainput"
%typemap(javain) String "$javainput"

%typemap(javaout) DataView {
  return $jnicall;
}
}
