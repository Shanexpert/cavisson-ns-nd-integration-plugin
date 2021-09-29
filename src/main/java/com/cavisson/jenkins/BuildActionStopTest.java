package com.cavisson.jenkins;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.logging.Level;
import java.util.logging.Logger;
import hudson.model.Run;
import hudson.model.ModelObject;
import hudson.model.TaskListener;
import hudson.model.Result;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.kohsuke.stapler.bind.JavaScriptMethod;

public class BuildActionStopTest implements ModelObject{

  private transient static final Logger logger = Logger.getLogger(BuildActionStopTest.class.getName());
    /**
   * The {@link NetStormStopAction} that this report belongs to.
   */
  private transient NetStormStopAction buildAction;
  private static Run<?, ?> currentBuild = null;
  private static int testrun = -1;
  public static String username = "";
  public static String portStr = "";
  
	BuildActionStopTest(final NetStormStopAction buildAction, TaskListener listener)throws IOException 
	  {
	    this.buildAction = buildAction;
//	    this.testrun = this.buildAction.getNetStormTest();
	    addPreviousBuildReportToExistingReport();
	  }
	
	public BuildActionStopTest(int testRun, String userName, String portStr) {
   	 logger.log(Level.INFO, "NetStormStopAction: testRun = " + testRun+", username = "+userName+", portStr = "+portStr);
   	BuildActionStopTest.testrun = testRun;
   	BuildActionStopTest.username = userName;
   	BuildActionStopTest.portStr = portStr;
   	logger.log(Level.INFO, "NetStormStopAction: after testRun = " + BuildActionStopTest.testrun+", username = "+BuildActionStopTest.username+", portStr = "+BuildActionStopTest.portStr);
   }
	
	@Override
    public String getDisplayName() {
		return "Stop"; 
    }
	
	public Run<?, ?> getBuild()
	  {
	    return buildAction.getBuild();
	  }
	
	private void addPreviousBuildReportToExistingReport() 
	  {
	    // Avoid parsing all builds.
	    if (BuildActionStopTest.currentBuild == null) 
	    {
	    	BuildActionStopTest.currentBuild = getBuild();
	    }
	    else
	    {
	      if (BuildActionStopTest.currentBuild != getBuild()) {
	    	  BuildActionStopTest.currentBuild = null;
	        return;
	      }
	    }

	    Run<?, ?> previousBuild = getBuild().getPreviousBuild();
	    if (previousBuild == null) {
	      return;
	    }

	    NetStormStopAction previousPerformanceAction = previousBuild.getAction(NetStormStopAction.class);
	    if (previousPerformanceAction == null) {
	      return;
	    }

	    BuildActionStopTest previousBuildActionResults = previousPerformanceAction.getBuildActionResultsDisplay();
	    if (previousBuildActionResults == null) {
	      return;
	    }

//	    NetStormReport lastReport = previousBuildActionResults.getNetStormReport();
//	    getNetStormReport().setLastBuildReport(lastReport);
	  }
	
	@JavaScriptMethod
    public String stopRunningTest()
    {
    	try{
    		logger.log(Level.INFO, "stopRunningTest: after testRun = " + BuildActionStopTest.testrun+", username = "+BuildActionStopTest.username+", portStr = "+BuildActionStopTest.portStr);
    		
    		logger.log(Level.INFO, "stopRunningTest: going to wait");
    		logger.log(Level.INFO, "stopRunningTest: get result = " + getBuild().getResult());
    		
    		Result result = getBuild().getResult();
    		if(result != null){
    			return "Test run is not running";
    		}
    		
    		while(BuildActionStopTest.testrun == -1){
    			Thread.sleep(5 * 1000);
    		}
    		
    		logger.log(Level.INFO, "stopRunningTest: wait is over");
    		
    		URL url;
    		String str = "";
    		url = new URL(BuildActionStopTest.portStr+"/ProductUI/productSummary/jenkinsService/stopTest?username="+BuildActionStopTest.username+"&testrun="+BuildActionStopTest.testrun);

    		   try{
    	  	HttpURLConnection connectt = (HttpURLConnection) url.openConnection();
    	  	connectt.setRequestMethod("GET");
    	  	connectt.setRequestProperty("Accept", "application/json");    

    	  	if (connectt.getResponseCode() != 200) {
    	  	  logger.log(Level.INFO, "stopRunningTest: Getting Error  = " + connectt.getResponseCode());
    	  	}

    	  	BufferedReader brr = new BufferedReader(new InputStreamReader(connectt.getInputStream()));
    	  	String res = brr.readLine();
    	  	
    	  	  
    	  	logger.log(Level.INFO, "stopRunningTest res = " + res);
    	  	
    	  	if(res.startsWith("Unable to stop test")){
    	  		str = res;
    	  	}else if(res.equals("stopInProgress")){
    	  		str = "Test is stopped successfully";
    	  	}else if(res.equals("stopped")){
    	  		str = "Test Run '"+BuildActionStopTest.testrun+"' is not running";
    	  	}else if(res.equals("errorInStop")){
    	  		str = "Unable to stop test";
    	  	}
    	  	
    	  	logger.log(Level.INFO, "stopRunningTest: build status = " + connectt.getResponseCode());
    	  	
    	  	getBuild().setResult(Result.ABORTED);
    	  	
    	    }catch(Exception ex){
    	    	logger.log(Level.SEVERE, "Unknown exception in stopping test -", ex);
    	    	return null;
    		}
    		   BuildActionStopTest.testrun = -1;
//       	  	NetStormStopAction.username = "";
//       	  	NetStormStopAction.portStr = "";
    		return str;
    	}catch(Exception e){
    		logger.log(Level.SEVERE, "Exception: Unable to stop running test. Error -" + e);
    		return null;
    	}
    }
}
