#!/bin/bash

# fetch and install the newrelic Agent SDK libs and includes
pushd /tmp
NR_SDK_VER=0.9.9.0
wget http://download.newrelic.com/agent_sdk/nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64.tar.gz
tar xvfz nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64.tar.gz
cd nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64
echo sudo cp include/*h /usr/local/include/
echo sudo cp lib/*so /usr/local/lib/
sudo ldconfig
popd
