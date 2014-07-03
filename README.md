Nginx module for newrelic reporting
===================================

This module is an nginx module for seemless reporting of every request to newrelic.

This enables you to monitor the latency and throughput of every nginx endpoint, including static content, native modules, lua modules, fastcgi, proxy requests and so on.

Reporting is done via the Newrelic Agent SDK.
Implementation was based on an initial work done by the Newrelic team. 

Sample Configuration
--------------------

Inside your main nginx.conf, or any enabled location, add on the http level config:
```
http {
	...
    newrelic on;
	newrelic_license_key my-newrelic-key;
	newrelic_app_name my-app-name;
```

This will enable newrelic transaction reporting on all defined servers and locations.
The default newrelic transaction name will be the endpoint uri (without parameters). For each location, you can change the transaction name by
```
http {
  ...
  server {
    ...
	location /some_location {
	    newrelic_transaction_name some_other_name;
	...
	}
	
	
	location /no_report_location {
		newrelic off; # turn off newrelic reporting for this location
		...
	}
```

Dependencies
------------
This module uses the [Newrelic Agent SDK](https://docs.newrelic.com/docs/agent-sdk/agent-sdk) for communicating with newrelic. 

[Download](http://download.newrelic.com/agent_sdk/) and install it by running the following:

```bash
NR_SDK_VER=0.9.9.0
wget http://download.newrelic.com/agent_sdk/nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64.tar.gz
tar xvfz nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64.tar.gz
cd nr_agent_sdk-v${NR_SDK_VER}-beta.x86_64
sudo cp include/*h /usr/local/include/
sudo cp lib/*so /usr/local/lib/
sudo ldconfig
```

Installation
------------

You will need to compile nginx from source in order to add this module.

- Clone this repository
- Install the Newrelic Agenst SDK - see [here](#Dependencies)
- [Download](http://nginx.org/en/download.html) nginx source, extract and cd to it
- Configure with
```
./configure --add-module=/path/to/this/folder
```
- Run `make && sudo make install`

Check the nginx documentation for other configure options (it usually defaults to `/opt/nginx`)

Limitations
-----------
Some of the information here was recevied from the newrelic team.

- Only 10K concurrent transactions are supported by the Newrelic Agent SDK
- The newrelic agent opens an additional thread for each worker, doing the harvesting and reporting. It reports once per minute.
- If you have internal locations or other non-standard locations, turn off newrelic for those locations by setting `newrelic off;`
