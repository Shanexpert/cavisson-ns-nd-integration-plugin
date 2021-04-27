cavisson-ns-nd-integration-plugin
=============

`Release:` https://github.com/jenkinsci/cavisson-ns-nd-integration-plugin/releases

A plugin for jenkins used to trigger a test suite on remote Netstorm server. 

Test suite controls the execution of different scenarios together for comparing actual outcome to the predicted outcome. It has SLAs configured, based on which, the test is either pass or fail. As an outcome of test suite execution, Jenkins contains a link to the brief report of the test.

More information can be found on the Wiki page https://plugins.jenkins.io/cavisson-ns-nd-integration/

Note : Plugin source code is hosted on [GitHub](https://github.com/jenkinsci/cavisson-ns-nd-integration-plugin).

How to build and test
=====================

* Configure the plugin

1 `Add build action(Execute NetStorm/NetCloud Test):`
    This step will make connection to the remote Netstorm server, execute the scenario/test suite and generate a test run number.
			  
2 `Add Post build action(NetStorm/NetCloud Performance Publisher):`
    This step will fetch the HTML report from the NetStorm server and publish it using HTML Publisher plugin.

See https://github.com/jenkinsci/cavisson-ns-nd-integration-plugin for details.
