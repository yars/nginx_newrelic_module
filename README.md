Nginx module for newrelic reporting
===================================

This module is an nginx module for seemless reporting of every request to newrelic.
This enables you to monitor the latency and throughput of every nginx endpoint, including static content, native modules, lua modules, fastcgi, proxy requests and so on.

Dependencies
------------
This module uses the [Newrelic Agent SDK](https://docs.newrelic.com/docs/agent-sdk/agent-sdk) for communicating with newrelic. 
[Download](http://download.newrelic.com/agent_sdk/) and install it by running the following:

`
NR_SDK_VER=0.9.9.0
wget http://download.newrelic.com/agent_sdk/nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64.tar.gz
tar xvfz nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64.tar.gz
cd nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64
sudo cp include/*h /usr/local/include/
sudo cp lib/*so /usr/local/lib/
sudo ldconfig
`
