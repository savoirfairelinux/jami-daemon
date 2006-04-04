# CVS Web: http://digium-cvs.netmonks.ca/viewcvs.cgi/libiax2/

date=20060404
lib=libiax2
if [ ! -d $lib ]; then
  echo "Welcome to the installation of $lib"
  echo -n "Do you want to extract the $date tar.gz or get from svn? [targz/svn]: "
  read response
  if [ $response == "svn" ]; then
    echo "Downloading libiax2 from source... (may take a long time)"
    svn checkout http://svn.digium.com/svn/libiax2/trunk libiax2
  else 
    file="$lib.svn$date.tar.gz"
    echo "Extracting libiax2 from $file..."
    tar xzvf $file
  fi
fi

cd libiax2
if [ ! -f configure ]; then
  ./gen.sh
fi

./configure && make
