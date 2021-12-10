package com.cavisson.jenkins;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.kohsuke.stapler.bind.JavaScriptMethod;

import hudson.model.ModelObject;
import hudson.model.Result;
import hudson.model.Run;
import hudson.model.TaskListener;

public class BuildActionStopThread implements ModelObject{

	  private transient static final Logger logger = Logger.getLogger(BuildActionStopThread.class.getName());
	    /**
	   * The {@link NetStormStopAction} that this report belongs to.
	   */
	  private transient NetStormStopThread buildAction;
	  private static Run<?, ?> currentBuild = null;
	  private static String job_id = "";
	  public static String username = "";
	  public static String portStr = "";
	  
	  BuildActionStopThread(final NetStormStopThread buildAction, TaskListener listener)throws IOException 
		  {
		    this.buildAction = buildAction;
//		    this.testrun = this.buildAction.getNetStormTest();
		    addPreviousBuildReportToExistingReport();
		  }
		
		public BuildActionStopThread(String job_id, String userName, String portStr) {
	   	 logger.log(Level.INFO, "NetStormStopThread: job_id = " + job_id+", username = "+userName+", portStr = "+portStr);
	   	BuildActionStopThread.job_id = job_id;
	   	BuildActionStopThread.username = userName;
	   	BuildActionStopThread.portStr = portStr;
	   	logger.log(Level.INFO, "NetStormStopAction: after testRun = " + BuildActionStopThread.job_id+", username = "+BuildActionStopThread.username+", portStr = "+BuildActionStopThread.portStr);
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
		    if (BuildActionStopThread.currentBuild == null) 
		    {
		    	BuildActionStopThread.currentBuild = getBuild();
		    }
		    else
		    {
		      if (BuildActionStopThread.currentBuild != getBuild()) {
		    	  BuildActionStopThread.currentBuild = null;
		        return;
		      }
		    }

		    Run<?, ?> previousBuild = getBuild().getPreviousBuild();
		    if (previousBuild == null) {
		      return;
		    }

		    NetStormStopThread previousPerformanceAction = previousBuild.getAction(NetStormStopThread.class);
		    if (previousPerformanceAction == null) {
		      return;
		    }

		    BuildActionStopThread previousBuildActionResults = previousPerformanceAction.getBuildActionResultsDisplay();
		    if (previousBuildActionResults == null) {
		      return;
		    }

//		    NetStormReport lastReport = previousBuildActionResults.getNetStormReport();
//		    getNetStormReport().setLastBuildReport(lastReport);
		  }
		
		@JavaScriptMethod
	    public String stopRunningTest(String jobid, String username, String url)
	    {
	    	try{
	    		logger.log(Level.INFO, "stopRunningTest: after job id = " + BuildActionStopThread.job_id+", username = "+BuildActionStopThread.username+", portStr = "+BuildActionStopThread.portStr);
	    		
	    		logger.log(Level.INFO, "stopRunningTest: going to wait");
	    		logger.log(Level.INFO, "stopRunningTest: get result = " + getBuild().getResult());
	    		
//	    		Result result = getBuild().getResult();
//	    		if(result != null){
//	    			return "Test run is not running";
//	    		}
	    		
//	    		while(BuildActionStopThread.testrun == -1){
//	    			Thread.sleep(5 * 1000);
//	    		}
	    		
	    		logger.log(Level.INFO, "stopRunningTest: wait is over");
	    		
	    		URL urll;
	    		String str = "";
	    		urll = new URL(url+"/ProductUI/productSummary/jenkinsService/stopMultipleTest?username="+username + "&Job_Id="+ jobid);

	    		   try{
	    	  	HttpURLConnection connectt = (HttpURLConnection) urll.openConnection();
	    	  	connectt.setRequestMethod("GET");
	    	  	connectt.setRequestProperty("Accept", "application/json");    

	    	  	if (connectt.getResponseCode() != 200) {
	    	  	  logger.log(Level.INFO, "stopRunningTest: Getting Error  = " + connectt.getResponseCode());
	    	  	}

	    	  	BufferedReader brr = new BufferedReader(new InputStreamReader(connectt.getInputStream()));
	    	  	String res = brr.readLine();
	    	  	
	    	  	  
	    	  	logger.log(Level.INFO, "stopRunningTest res = " + res);
	    	  	str = res;
//	    	  	if(res.startsWith("Unable to stop test")){
//	    	  		str = res;
//	    	  	}else if(res.equals("stopInProgress")){
//	    	  		str = "Test is stopped successfully";
//	    	  	}else if(res.equals("stopped")){
//	    	  		str = "Test Run '"+BuildActionStopThread.testrun+"' is not running";
//	    	  	}else if(res.equals("errorInStop")){
//	    	  		str = "Unable to stop test";
//	    	  	}
	    	  	
	    	  	logger.log(Level.INFO, "stopRunningTest: build status = " + connectt.getResponseCode());
	    	  	
	    	  	getBuild().setResult(Result.ABORTED);
	    	  	
	    	    }catch(Exception ex){
	    	    	logger.log(Level.SEVERE, "Unknown exception in stopping test -", ex);
	    	    	return null;
	    		}
	    		//   BuildActionStopThread.testrun = -1;
//	       	  	NetStormStopAction.username = "";
//	       	  	NetStormStopAction.portStr = "";
	    		return str;
	    	}catch(Exception e){
	    		logger.log(Level.SEVERE, "Exception: Unable to stop running test. Error -" + e);
	    		return null;
	    	}
	    }
	}
