#!/bin/sh
# This code buinds the userland part of coda. 
# Script by Dimitri Tcaciuc <dtcaciuc@gmail.com>, 
#  augmented by Christer Bernérus <bernerus@medic.chalmers.se>
# To avoid mess, I prefer installing into my homedirectory
prefix=$HOME/work/proj/coda

export PATH=$PATH:$prefix:$prefix/bin

echo '-- Installing into ' $prefix

# Using tmp to create a mess
cd /var/tmp
if [ ! -d codatmp ] ; then
   mkdir codatmp
fi

cd codatmp/

# Installing readline
echo '-- Compiling readline'
curl -O ftp://ftp.gnu.org/gnu/readline/readline-4.3.tar.gz
tar xvfz readline-4.3.tar.gz
curl -O ftp://ftp.gnu.org/gnu/readline/readline-4.3-patches/readline43-001
curl -O ftp://ftp.gnu.org/gnu/readline/readline-4.3-patches/readline43-002
curl -O ftp://ftp.gnu.org/gnu/readline/readline-4.3-patches/readline43-003
curl -O ftp://ftp.gnu.org/gnu/readline/readline-4.3-patches/readline43-004
curl -O ftp://ftp.gnu.org/gnu/readline/readline-4.3-patches/readline43-005
cd readline-4.3
patch -p0 < ../readline43-001
patch -p0 < ../readline43-002
patch -p0 < ../readline43-003
patch -p0 < ../readline43-004
patch -p0 < ../readline43-005
./configure --disable-shared --prefix=$prefix 
make 
make install
cd ..
rm -rf readline*

# Installing lwp
echo '-- Compiling LWP 1.11'
curl -O http://www.coda.cs.cmu.edu/pub/coda/src/lwp-1.11.tar.gz
tar xvfz lwp-1.11.tar.gz
cd lwp-1.11
./configure --prefix=$prefix 
make 
make check 
make install
cd ..
rm -rf lwp*

# Installing rvm
echo '-- Compiling RVM 1.9' 
curl -O http://www.coda.cs.cmu.edu/pub/coda/src/rvm-1.9.tar.gz
tar xvfz rvm-1.9.tar.gz
cd rvm-1.9
./configure --prefix=$prefix 
make 
make check 
make install
cd ..
rm -rf rvm*

# Installing rpc
echo '-- Compiling RPC2 1.22'
curl -O http://www.coda.cs.cmu.edu/pub/coda/src/rpc2-1.22.tar.gz
tar xvfz rpc2-1.22.tar.gz
cd rpc2-1.22
./configure --prefix=$prefix
make
make check
make install
cd ..
rm -rf rpc*

# Installing CODA
echo '-- Compiling CODA'
curl -O http://www.coda.cs.cmu.edu/pub/coda/src/coda-6.0.6.tar.gz
tar xvfz coda-6.0.6.tar.gz
cd coda-6.0.6
./configure --with-rvm=$prefix --with-lwp=$prefix \
    --with-rpc2=$prefix

echo "#define __BSD44__" >>coda-src/venus/worker.h

# Checking for needed resolv.h
if [ ! -f /usr/include/resolv.h ] ; then
    echo '-- No file found, aborting'; exit 1
fi
echo '-- Found: /usr/include/resolv.h ...'

echo "#define HAVE_RESOLV_H 1" >>config.h
echo 'LIBS:= $(LIBS) -lresolv' >>Makeconf
make

echo '-- Installing coda client and server to your drive'
echo '-- warning: default location is /usr/local'
echo '-- Press enter to continue'
read CONTINUE
sudo make client-install
sudo make server-install

echo '*********************************************'
echo '** If you haven''t seen any errors, then'
echo '   CODA is successfully installed to'
echo '   '$prefix' without any'
echo '   problems'
echo ''

if [ -f $HOME/.bashrc ]; then
    if grep "$prefix" $HOME/.bashrc >/dev/null 2>&1; then
        :
    else
        echo "export PATH=\${PATH}:$prefix:$prefix/bin:$prefix/sbin" \
            >> $HOME/.bashrc
        echo '**'"I've just added this line to your .bashrc"
        echo "export PATH=\${PATH}:$prefix:$prefix/bin:$prefix/sbin"
    fi
fi

if [ -f $HOME/.tcshrc ]; then
    if grep "$prefix" $HOME/.tcshrc >/dev/null 2>&1; then
        :
    else
	echo setenv PATH "\${PATH}:$prefix:$prefix/bin:$prefix/sbin" \
	    >>$HOME/.tcshrc
        echo '**'"I've just added this line to your .tcshrc"
	echo setenv PATH "\${PATH}:$prefix:$prefix/bin:$prefix/sbin" 
    fi
fi

echo '   If you''re using any other shell, add a similar line'
echo '   to the proper config file.'
echo ''
echo '** Next steps:'
echo '     To test the installation and connect to the test server,'
echo '     do the following:'
echo ''
echo '     $ sudo '$prefix'/sbin/venus-setup testserver.coda.cs.cmu.edu 20000'
echo ''
echo '   At this point you should have your kernel extension locked and loaded'
echo '   You can check if its loaded by doing'
echo '     $ kextstat | grep coda'
echo ''
echo '   Next, fire up venus daemon'
echo '     $ sudo '$prefix'/sbin/venus &'
echo ''
echo '   You should now be able to see /coda directory'
echo '     $ ls /coda'
echo ''
echo '   If the only thing that you see is a file named NOT_REALLY_CODA,'
echo '   then something is b0rked. Otherwise, you can try connecting to'
echo '   CMU test server that is opened to anyone:'
echo '     $ ls /coda/testserver.coda.cs.cmu.edu'
echo ''
echo '   Naturally, do to the above, you need to be connected to Internet'
echo ''
echo '   Good luck and have fun!'

# ze end

