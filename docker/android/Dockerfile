FROM ubuntu:15.10

# Never ask for confirmations
ENV DEBIAN_FRONTEND noninteractive
RUN echo "debconf shared/accepted-oracle-license-v1-1 select true" | debconf-set-selections
RUN echo "debconf shared/accepted-oracle-license-v1-1 seen true" | debconf-set-selections

RUN apt-get update && ( apt-get upgrade -y || apt-get upgrade -y || apt-get upgrade -y )
RUN apt-get install -y software-properties-common python-software-properties bzip2 unzip openssh-client git lib32stdc++6 lib32z1 --fix-missing

# Add oracle-jdk7 to repositories
RUN add-apt-repository ppa:webupd8team/java

# Install oracle-jdk8
RUN apt-get update
RUN apt-get -y install oracle-java8-installer oracle-java8-set-default

RUN wget http://dl.google.com/android/android-sdk_r24-linux.tgz
RUN tar -xvzf android-sdk_r24-linux.tgz
RUN mv android-sdk-linux /usr/local/android-sdk
RUN rm android-sdk_r24-linux.tgz

# Install Android tools
RUN ( sleep 5 && while [ 1 ]; do sleep 1; echo y; done ) | /usr/local/android-sdk/tools/android update sdk --filter tool,platform-tool,platform-23,android-23,extra,addon-google_apis-google-23,addon-google_apis_x86-google-23,build-tools-23.1.0 --no-ui -a

# Install Android NDK
RUN wget http://dl.google.com/android/ndk/android-ndk-r10e-linux-x86_64.bin
RUN chmod a+x android-ndk-r10e-linux-x86_64.bin
RUN ./android-ndk-r10e-linux-x86_64.bin
RUN mv android-ndk-r10e /usr/local/android-ndk
RUN rm android-ndk-r10e-linux-x86_64.bin

# Install Gradle
RUN wget https://downloads.gradle.org/distributions/gradle-2.8-bin.zip
RUN unzip gradle-2.8-bin.zip
RUN mv gradle-2.8 /usr/local/gradle
RUN rm gradle-2.8-bin.zip

# Environment variables
ENV ANDROID_HOME /usr/local/android-sdk
ENV ANDROID_SDK_HOME $ANDROID_HOME
ENV ANDROID_NDK_HOME /usr/local/android-ndk
ENV GRADLE_HOME /usr/local/gradle
ENV PATH $PATH:$ANDROID_SDK_HOME/tools
ENV PATH $PATH:$ANDROID_SDK_HOME/platform-tools
ENV PATH $PATH:$ANDROID_NDK_HOME
ENV PATH $PATH:$GRADLE_HOME/bin

# Export JAVA_HOME variable
ENV JAVA_HOME /usr/lib/jvm/java-8-oracle
