/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.security.KeyManagementException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.X509Certificate;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.Iterator;
import java.util.Set;
import java.util.TimeZone;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSession;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import net.sf.json.JSONArray;
import net.sf.json.JSONObject;
import net.sf.json.JSONSerializer;
import org.json.simple.parser.JSONParser;

import hudson.FilePath;
import hudson.util.Secret;

/**
 *
 * @author richa.garg
 */
public class NSNDIntegrationConnectionManager {

  private final String nsUrlConnectionString;
  private String nsUsername = "";
  private Secret nsPassword;
  private final String ndUrlConnectionString;
  private URLConnection urlConn = null;
  private transient final Logger logger = Logger.getLogger(NSNDIntegrationConnectionManager.class.getName());
  private String ndUsername = "";
  private Secret ndPassword;
  NSNDIntegrationParameterForReport ndParam;
  private ObjectOutputStream oos = null;
  private ObjectInputStream ois = null;
  private String restUrlNS = "";
  private String restUrlND = "";
  private String resultNS;
  private String resultND;
  private String curStart;
  private String curEnd;
  private JSONObject jkRule = new JSONObject();
  private String critical;
  private String warning;
  private String overall;
  private String project = "";
  private String subProject = "";
  private String scenario = "";
  private String scenarioName = "NA";
  private PrintStream consoleLogger = null;
  private String pollReportURL;
  private JSONObject resonseReportObjNS = null;
  private JSONObject resonseReportObjND = null;
  private static int POLL_CONN_TIMEOUT = 60000;
  private static int POLL_REPEAT_TIME = 1 * 60000;
  private static int POLL_REPEAT_FOR_REPORT_TIME= 30000;
  private static int INITIAL_POLL_DELAY = 60000;
  private int testRun = -1;
  private String err = "Connection failure, please check whether Connection URI is specified correctly";

  public String getCritical() {
    return critical;
  }

   public String getProject() {
    return project;
  }

  public void setProject(String project) {
    this.project = project;
  }
  
  public String getSubProject() {
    return subProject;
  }

  public void setSubProject(String subProject) {
    this.subProject = subProject;
  }
  
   public String getScenario() {
    return scenario;
  }

  public void setScenario(String scenario) {
    this.scenario = scenario;
  }
  
  
  public void setCritical(String critical) {
    this.critical = critical;
  }

  public String getWarning() {
    return warning;
  }

  public void setWarning(String warning) {
    this.warning = warning;
  }

  public String getOverall() {
    return overall;
  }

  public void setOverall(String overall) {
    this.overall = overall;
  }

  public JSONObject getJkRule() {
    return jkRule;
  }

  public void setJkRule(JSONObject jkRule) {
    this.jkRule = jkRule;
  }

  public String getCurStart() {
    return curStart;
  }

  public void setCurStart(String curStart) {
    this.curStart = curStart;
  }

  public String getCurEnd() {
    return curEnd;
  }

  public void setCurEnd(String curEnd) {
    this.curEnd = curEnd;
  }

  public String getResultNS() {
    return resultNS;
  }

  public void setResultNS(String result) {
    this.resultNS = result;
  }

  public String getResultND() {
    return resultND;
  }

  public void setResultND(String result) {
    this.resultND = result;
  }
  
  static
  { 
    disableSslVerification();
  }	

  private static void disableSslVerification() 
  {
    try
    {
      // Create a trust manager that does not validate certificate chains
      TrustManager[] trustAllCerts = new TrustManager[] {new X509TrustManager() 
      {            
	public java.security.cert.X509Certificate[] getAcceptedIssuers()
	{                
	  return null;            
	}

	public void checkClientTrusted(X509Certificate[] certs, String authType) 
	{ 
	}            

	public void checkServerTrusted(X509Certificate[] certs, String authType)
	{            
	}        
      }        
      };
      // Install the all-trusting trust manager       
      SSLContext sc = SSLContext.getInstance("SSL");       
      sc.init(null, trustAllCerts, new java.security.SecureRandom());   
      HttpsURLConnection.setDefaultSSLSocketFactory(sc.getSocketFactory());  
      // Create all-trusting host name verifier    
      HostnameVerifier allHostsValid = new HostnameVerifier() 
      {  
	public boolean verify(String hostname, SSLSession session) 
	{ 
	  return true;            
	}         
      };        
      // Install the all-trusting host verifier        
      HttpsURLConnection.setDefaultHostnameVerifier(allHostsValid);    
    }
    catch (NoSuchAlgorithmException e) 
    {     
    } 
    catch (KeyManagementException e)
    {       
    }
  }

  public NSNDIntegrationConnectionManager(String nsUrlConnectionString, String nsUsername, Secret nsPassword, String ndUrlConnectionString, String ndUsername, Secret ndPassword, NSNDIntegrationParameterForReport ndParam) {

    this.nsUrlConnectionString = nsUrlConnectionString;
    this.ndUrlConnectionString = ndUrlConnectionString;
    this.nsUsername = nsUsername;
    this.ndUsername = ndUsername;
    this.ndParam = ndParam;
    this.restUrlNS = nsUrlConnectionString;
    this.restUrlND = ndUrlConnectionString;
    this.nsPassword = nsPassword;
    this.ndPassword = ndPassword;

  }

  /**
   * This Method makes connection to ns and nd.
   *
   * @param errMsg
   * @return true , if Successfully connected and authenticated false ,
   * otherwise
   */
  public boolean testNDConnection(StringBuffer errMsg, String test) {
    logger.log(Level.INFO, "testNDConnection() method is  called. rest url -" + restUrlND);

    /* for checking connection*/
    if (checkAndMakeConnection(ndUrlConnectionString, restUrlND, errMsg, test) && checkAndMakeConnectionNS(nsUrlConnectionString, errMsg)) {
      logger.log(Level.INFO, "After check connection method for in integrated.");

       JSONObject jsonResponse  =  (JSONObject) JSONSerializer.toJSON(getResultNS());
       JSONObject jsonResponseND  =  (JSONObject) JSONSerializer.toJSON(getResultND());
       
      if (getResultNS() == null) {
        logger.log(Level.INFO, "Connection failure, please check whether Connection URI of Netstorm is specified correctly");
        errMsg.append("Connection failure, please check whether Connection URI is specified correctly");
        return false;
      } else if (getResultND() == null) {
        logger.log(Level.INFO, "Connection failure, please check whether Connection URI of Netdiganostic  is specified correctly");
        errMsg.append("Connection failure, please check whether Connection URI is specified correctly");
        return false;
      } else {
        
         if(!jsonResponse.get("errMsg").equals("") || !jsonResponseND.get("errMsg").equals(""))
          {
            String nsErr = "";
            String ndErr = "";
            
            if(!jsonResponse.get("errMsg").equals(""))
              nsErr = (String) jsonResponse.get(jsonResponse.get("errMsg")) + " for netstorm.";
            
            if(!jsonResponseND.get("errMsg").equals(""))
              ndErr = (String) jsonResponseND.get("errMsg") +" for netdiagnostics . ";
            
            err = nsErr +" "+ ndErr;
            logger.log(Level.INFO, "Connection failure, please check whether Connection URI of either NetStorm or Netdiagnostic is specified correctly");
            errMsg.append(err);
            return false;
          }
         else
          {
            logger.log(Level.INFO, "Successfully Authenticated.");
            return true;
          }
      }
      
   
    } else {
      logger.log(Level.INFO, "Connection failure, please check whether Connection URI of either NetStorm or Netdiagnostic is specified correctly");
      errMsg.append(err);
      return false;
    }
  }

  /*Method to check connection for netstorm*/
  private boolean checkAndMakeConnectionNS(String urlString, StringBuffer errMsg) {
    logger.log(Level.INFO, "checkAndMakeConnectionNS method called. with arguments restUrl for netstorm  : ", new Object[]{urlString});
    try {
      JSONObject reqObj = new JSONObject();
      reqObj.put("username", this.nsUsername);
      reqObj.put("password", this.nsPassword.getPlainText());
      reqObj.put("URLConnectionString", this.nsUrlConnectionString);
      disableSslVerification();
      URL url;
      String str = getUrlString(urlString); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
      url = new URL(str + "ProductUI/productSummary/jenkinsService/validateUser");

      logger.log(Level.INFO, "checkAndMakeConnectionNS method for netstorm  called. with arguments url = " + url);
      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
      conn.setRequestMethod("POST");

      conn.setRequestProperty("Accept", "application/json");
      conn.setDoOutput(true);
      String json = reqObj.toString();
      OutputStream os = conn.getOutputStream();
      os.write(json.getBytes());
      os.flush();

      if (conn.getResponseCode() != 200) {
        throw new RuntimeException("Failed : HTTP error code : " + conn.getResponseCode());
      } else {
        BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
        setResultNS(br.readLine());
        logger.log(Level.INFO, "RESPONSE from Netstorm -> " + getResultNS());
        return true;
      }

    } catch (MalformedURLException e) {
      logger.log(Level.SEVERE, "Unknown exception in establishing connection with Netstorm Machine. MalformedURLException -", e);
      return false;
    } catch (IOException e) {
      logger.log(Level.SEVERE, "Unknown exception in establishing connection with Netstorm Machine. IOException -", e);
      return false;
    } catch (Exception e) {
      logger.log(Level.SEVERE, "Unknown exception in establishing connection with Netstorm Machine.", e);
      return (false);
    }
  }

  /*Getting url string to use for connection making*/
  public String getUrlString(String urlString) {
    String urlAddrs = "";
    try {
      String str[] = urlString.split(":");
      if (str.length > 2) {
        urlAddrs = str[0] + ":" + str[1];
        if (str[2].contains("/")) {
          String value[] = str[2].split("/");
          urlAddrs = urlAddrs + ":" + value[0];

        } else {
          urlAddrs = urlAddrs + ":" + str[2];
        }
      }
      else
     {
        urlAddrs = urlString;
     }
      
       /*Checking for / at end.*/
      if (! urlAddrs.trim().endsWith("/")) {
	urlAddrs = urlAddrs + "/";
      }
      
    } catch (Exception ex) {
      logger.log(Level.SEVERE, "Error in getting url string ");
      return urlString.substring(0, urlString.lastIndexOf("/"));
    }

    return urlAddrs;
  }

  private boolean checkAndMakeConnection(String urlString, String restUrl, StringBuffer errMsg) {
    return checkAndMakeConnection(urlString, restUrl, errMsg, null);
  }

  /**
   * This method checks connection with netstorm
   *
   * @param urlString
   * @param servletPath
   * @param errMsg
   * @param aa
   * @return true if connection successfully made false, otherwise
   */
  private boolean checkAndMakeConnection(String urlString, String restUrl, StringBuffer errMsg, String test) {
    logger.log(Level.INFO, "checkAndMakeConnection method for NetDiagnostics called. with arguments restUrl : ", new Object[]{restUrl});
    try {
      if (ndParam != null) {
        logger.log(Level.INFO, "checkAndMakeConnection method for NetDiagnostics called. with ndParam " + ndParam.toString());
      //  ndParam.setCurStartTime(getCurStart().replace(" ", "@"));
      //  ndParam.setCurEndTime(getCurEnd().replace(" ", "@"));

        if(ndParam.getBase1StartTime().equals(""))
            ndParam.setBase1StartTime(null);
          else
            ndParam.setBase1StartTime(ndParam.getBase1StartTime().replace(" ", "@"));
          
          if(ndParam.getBase1EndTime().equals(""))
            ndParam.setBase1EndTime(null);
          else
            ndParam.setBase1EndTime(ndParam.getBase1EndTime().replace(" ", "@"));
          
          if (ndParam.getBase2StartTime() != null && !ndParam.getBase2StartTime().equals("")) 
            ndParam.setBase2StartTime(ndParam.getBase2StartTime().replace(" ", "@"));
          else
        	ndParam.setBase2StartTime(null);

          if (ndParam.getBase2EndTime() != null && !ndParam.getBase2EndTime().equals("")) 
            ndParam.setBase2EndTime(ndParam.getBase2EndTime().replace(" ", "@"));
          else
        	ndParam.setBase2EndTime(null);
          
          if (ndParam.getBase3StartTime() != null && !ndParam.getBase3StartTime().equals("")) 
            ndParam.setBase3StartTime(ndParam.getBase3StartTime().replace(" ", "@"));
          else
          	ndParam.setBase3StartTime(null);

          if (ndParam.getBase3EndTime() != null && !ndParam.getBase3EndTime().equals("")) 
            ndParam.setBase3EndTime(ndParam.getBase3EndTime().replace(" ", "@"));
          else
          	ndParam.setBase3EndTime(null);
      }

      if (this.getCritical() != null && this.getCritical() != "") {
        ndParam.setCritiThreshold(this.getCritical().replace(" ", "@"));
      }

      if (this.getWarning() != null && this.getWarning() != "") {
        ndParam.setWarThreshold(this.getWarning().replace(" ", "@"));
      }

      if (this.getOverall() != null && this.getOverall() != "") {
        ndParam.setFailThreshold(this.getOverall().replace(" ", "@"));
      }

      URL url;

      String str = getUrlString(urlString);
      logger.log(Level.INFO, "value of test in case ND connection..." + test);
      if (test == null || test.equals(null)) {
        url = new URL(str + "ProductUI/productSummary/jenkinsService/reportData?&reportParam=" + ndParam.toString() + "&status=false" + "&chkRule=" + this.getJkRule());
      } else {
        url = new URL(str + "ProductUI/productSummary/jenkinsService/reportData?&reportParam=" + test + "&status=true"); //for only test connection
      }
      logger.log(Level.INFO, "checkAndMakeConnection method for NetDiagnostics called. with arguments url - " + url);
      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
      conn.setRequestMethod("GET");
      conn.setRequestProperty("Accept", "application/json");
      if (conn.getResponseCode() != 200) {
        throw new RuntimeException("Failed : HTTP error code : " + conn.getResponseCode());
      }

      BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
      setResultND(br.readLine());
      logger.log(Level.INFO, "RESPONSE from Netdiagnostics -> " + getResultND());

      JSONObject resonseObj = null;
      resonseObj = (JSONObject) JSONSerializer.toJSON(this.resultND);
       Boolean status = (Boolean)resonseObj.get("status");
      
//       if(status == false && !((String)resonseObj.get("errMsg")).equals(""))
//          err = (String)resonseObj.get("errMsg");
      
      return status;
    } catch (MalformedURLException e) {
      logger.log(Level.SEVERE, "Unknown exception in establishing connection with Netdiagnostics. MalformedURLException -", e);
      return false;
    } catch (IOException e) {
      logger.log(Level.SEVERE, "Unknown exception in establishing connection with Netdiagnostics. IOException -", e);
      return false;
    } catch (Exception e) {
      logger.log(Level.SEVERE, "Unknown exception in establishing connection with Netdiagnostics.", e);
      return (false);
    }
  }
  
  
  /**
   * Method is used for checking connection with netstorm and polling report from netstorm.
   * @param TestRun
   * @param consoleLogger
   */
  private void connectNSAndPollJsonReport(){
   
     try {

     logger.log(Level.INFO, "Test is stopped. Now getting report from Netstorm. It may take some time. URL = " + pollReportURL);
     
      /* Creating the thread. */
      Runnable pollReportState = new Runnable()
      {
        public void run()
        {
          try {

            /*Keeping flag based on report status on server.*/
            boolean isReportGenerated = true;
            
            logger.log(Level.INFO, "Starting Polling to server.");
            
            /*Running Thread till test stopped.*/
            while (isReportGenerated) {
              try {
        	
        	/*Creating Polling URL.*/
        	String pollURLWithArgs = pollReportURL + "?&testRun=" + testRun;    	
        	URL url = new URL(pollURLWithArgs);
        	HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        	conn.setConnectTimeout(POLL_CONN_TIMEOUT);
        	conn.setReadTimeout(POLL_CONN_TIMEOUT);
        	conn.setRequestMethod("GET");
        	conn.setRequestProperty("Accept", "application/json");    

        	if (conn.getResponseCode() != 200) {
        	  logger.log(Level.INFO, "Getting Error code on polling  = " + conn.getResponseCode() + ". Retrying in next poll in 5 minutes.");
        	}

        	BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));
        	String pollResString = br.readLine();
        	
        	try {
        	  
        	  logger.log(Level.INFO, "Polling Response = " + pollResString);
        	  JSONObject pollResponse = (JSONObject) JSONSerializer.toJSON(pollResString);
        	      	  
        	  /*Getting data, if not available.*/
        	 // logger.log(Level.INFO, "Getting data as - "+pollResponse);
                          	          	  
        	  if(pollResponse.getBoolean("status")) {
        	    /*Terminating Loop when test is stopped.*/
        	    isReportGenerated = false;
                    
                    /*Parsing to get the data from response. */
//                      JSONParser parser = new JSONParser();
//                      org.json.simple.JSONObject objJson = (org.json.simple.JSONObject) parser.parse(pollResString);
//                       String strData = objJson.get("data").toString();
//                      
//                       org.json.simple.JSONObject objJson2 =(org.json.simple.JSONObject)parser.parse(strData);
//                       String strData2 = objJson2.toJSONString();
//                       logger.log(Level.INFO, "Data -- = " + strData);
                 //      resonseReportObjNS = (JSONObject) JSONSerializer.toJSON(strData2);
        	    resonseReportObjNS = pollResponse;
        	  }
                  
        	} catch (Exception e) {
        	  logger.log(Level.SEVERE, "Error in parsing polling response = " + pollResString, e);
        	}

        	/*Closing Stream.*/
        	try {
        	  br.close();
        	} catch (Exception e) {
        	  logger.log(Level.SEVERE, "Error in closing stream inside polling thread.", e);
        	}
              } catch (Exception e) {
                logger.log(Level.SEVERE, "Error in polling report with interval. Retrying after 5 sec.", e);
              }

              /*Repeating till Test Ended.*/
              try {
        	Thread.sleep(POLL_REPEAT_FOR_REPORT_TIME);
                logger.log(Level.INFO, "Report generation is  in progress. Going to check on server. Time = " + new Date());
              } catch (Exception ex) {       	
        	logger.log(Level.SEVERE, "Error in polling connection in loop", ex);
              } 
            }

          } catch (Exception e) {
            logger.log(Level.SEVERE, "Error in polling report with interval.", e);
          }
        }   
      };

      // Creating and Starting thread for Getting Graph Data.
      Thread pollTestRunThread = new Thread(pollReportState, "pollTestRunThread");

      // Running it with Executor Service to provide concurrency.
      ExecutorService threadExecutorService = Executors.newFixedThreadPool(1);

      // Executing thread in thread Pool.
      threadExecutorService.execute(pollTestRunThread);

      // Shutting down in order.
      threadExecutorService.shutdown();

      // Checking for running state.
      // Wait until thread is not terminated.
      while (!threadExecutorService.isTerminated())
      {
      }    
      
    } catch (Exception e) {
      logger.log(Level.SEVERE, "Error in polling report.", e);
    }
   
  }

  

  public boolean fetchMetricData(NSNDIntegrationConnectionManager connection, String metrics[], String duration, int groupIds[], int graphIds[], int testRun, String testMode, FilePath fp) {
    JSONObject jsonRequest = makeRequestObject("GET_DATA");
    jsonRequest.put("TESTRUN", String.valueOf(testRun));
    jsonRequest.put(JSONKeys.TESTMODE.getValue(), testMode);
     jsonRequest.put(JSONKeys.PROJECT.getValue(), connection.getProject());
    jsonRequest.put(JSONKeys.SUBPROJECT.getValue(), connection.getSubProject());
    jsonRequest.put(JSONKeys.SCENARIO.getValue(), connection.getScenario());
    jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), nsUrlConnectionString);   
    this.testRun = testRun; 

    JSONArray jSONArray = new JSONArray();
    for (int i = 0; i < metrics.length; i++) {
      jSONArray.add(groupIds[i] + "." + graphIds[i]);
    }
    jsonRequest.put("Metric", jSONArray);

    logger.log(Level.INFO, "fetchMetricData() called. get result for netstorm " + getResultNS());
    logger.log(Level.INFO, "fetchMetricData() called. get result for netdiagnostic " + getResultND());
    JSONObject resonseObjNS = null;
    JSONObject resonseObjND = null;

    boolean nsStatus = false;
    boolean ndStatus = false;
    StringBuffer errMsg = new StringBuffer();
  //  if (checkAndMakeConnectionNS(nsUrlConnectionString, errMsg)) {
      try {
        URL url;
        String str = getUrlString(nsUrlConnectionString); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
        url = new URL(str + "ProductUI/productSummary/jenkinsService/jsonData");

        
        
        logger.log(Level.INFO, "Making connection with netstorm for metric  url - " + url);
        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
        conn.setRequestMethod("POST");

        conn.setRequestProperty("Accept", "application/json");

        String json = jsonRequest.toString();
        conn.setDoOutput(true);
        OutputStream os = conn.getOutputStream();
        os.write(json.getBytes());
        os.flush();

        if (conn.getResponseCode() != 200) {
          throw new RuntimeException("Failed : HTTP error code : " + conn.getResponseCode());
        }

        BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
        resonseReportObjNS =  (JSONObject) JSONSerializer.toJSON(br.readLine());
        if(resonseReportObjNS.containsKey("status"))
          {
            if(resonseReportObjNS.getBoolean("status") == false)
             {
                logger.log(Level.SEVERE, "Not able to get response form server due to some reason");
                consoleLogger.println("Error in report generation.");
                nsStatus = false;
                return false;
              } else {
            	  if(resonseReportObjNS.containsKey("Current Start Time") && resonseReportObjNS.containsKey("Current End Time")) {
            	   setCurStartAndEndDateTime(resonseReportObjNS.getString("Current Start Time"), resonseReportObjNS.getString("Current End Time"), connection.getScenario());
            	  }
              }
          }
        
          pollReportURL = str+"ProductUI/productSummary/jenkinsService/checkNetstormReportStatus";
        
          logger.log(Level.INFO, "url for polling report - pollReportURL = " + pollReportURL);
            
           connectNSAndPollJsonReport();
        
//          if(resonseReportObjNS == null)
//           {
//              logger.log(Level.SEVERE, "Not able to get response form server due to: " + errMsg);
//              return null;
//           }
//        
//        JSONObject jsonTestReportWholeObj = resonseReportObjNS.getJSONObject("testReport");
//        JSONObject jsonTestReport = jsonTestReportWholeObj.getJSONObject("members");
 //       String curStartTime = jsonTestReport.getString("Current Date Time");
 //       String curEndTime = jsonTestReport.getString("Current End Time");
 //       setCurStartAndEndDateTime(curStartTime, curEndTime, connection.getScenario());

           if(resonseReportObjNS.getBoolean("status")) {
        	   getHTMLReport("NS", fp, String.valueOf(testRun));
           }
      } catch (Exception e) {
        logger.log(Level.SEVERE, "Unknown exception in establishing connection with Netstorm .", e);
      }
//    }

    if (checkAndMakeConnection(ndUrlConnectionString, restUrlND, errMsg, null)) {
   
   // To get the ND Report 
//   try{
//        URL url;
//        String str = getUrlString(ndUrlConnectionString); 
//        url = new URL(str + "/ProductUI/productSummary/jenkinsService/reportData?&reportParam=" + ndParam.toString() + "&status=false" + "&chkRule=" + this.getJkRule());   
//   }
//   catch(Exception e){
//     logger.log(Level.INFO, "Connection to netdiagnostics unsuccessful, cannot to proceed to generate report.");
//     logger.log(Level.SEVERE, "Error: " + errMsg);
//   }
      logger.log(Level.INFO, "RESPONSE from netdiagnostics  -- " + this.resultND);
      resonseObjND = (JSONObject) JSONSerializer.toJSON(this.resultND);
      ndStatus = true;
   	   getHTMLReport("ND", fp, "-1");
    } else {
      logger.log(Level.INFO, "Connection to netdiagnostics unsuccessful, cannot to proceed to generate report.");
      logger.log(Level.SEVERE, "Error: " + errMsg);
      ndStatus = false;
    }

   // return parseJSONDataND(resonseReportObjNS, resonseObjND, testMode);
    if(nsStatus == true && ndStatus == true) {
    	return true;
    } else {
    	return false;
    }
  }
  
  private boolean getHTMLReport(String type, FilePath fp, String testRun) {
 	 
	  /*path of directory i.e. /var/lib/jenkins/workspace/jobName*/
	  String zipFile = fp + "/TestSuiteReport.zip";
	  logger.log(Level.INFO, "Pdf directory"+zipFile);
	 
	  File file = new File(zipFile);
//	  if(file.exists()) {
//		  file.delete();
//	  }
	  
	  try {
		  URL urlForHTMLReport;
		  String str = "";
		  if(type.equals("NS")) {
			 str = nsUrlConnectionString; 
		  } else {
			  str = ndUrlConnectionString;
		  }
		  urlForHTMLReport = new URL(str+"/ProductUI/productSummary/jenkinsService/getHTMLReport");
		  logger.log(Level.INFO, "urlForPdf-"+urlForHTMLReport);

		  HttpURLConnection connect = (HttpURLConnection) urlForHTMLReport.openConnection();
		  connect.setConnectTimeout(0);
		  connect.setReadTimeout(0);
		  connect.setRequestMethod("POST");
		  connect.setRequestProperty("Content-Type", "application/octet-stream");

		  connect.setDoOutput(true);
		  java.io.OutputStream outStream = connect.getOutputStream();
		  outStream.write(testRun.getBytes());
		  outStream.flush();

		  if (connect.getResponseCode() == 200) {
			  logger.log(Level.INFO, "response 200 OK");   
			  byte[] mybytearray = new byte[1024];
			  InputStream is = connect.getInputStream();
			  FileOutputStream fos = new FileOutputStream(file);
			  BufferedOutputStream bos = new BufferedOutputStream(fos);
			  int bytesRead;
			  while((bytesRead = is.read(mybytearray)) > 0){
				logger.log(Level.INFO, "bytesRead inside while check"+ bytesRead);
				bos.write(mybytearray, 0, bytesRead);
			  }
			  bos.close();
			  is.close();
		  } else {
			  logger.log(Level.INFO, "ErrorCode-"+ connect.getResponseCode());
			  logger.log(Level.INFO, "content type-" + connect.getContentType());
		  }
		  
		  String destDir = fp + "/TestSuiteReport";
          unzip(zipFile, destDir);
		  return true;
	  } catch (Exception e){
		  e.printStackTrace();
		  return false;
	  }
	  
  }
  
  private static void unzip(String zipFilePath, String destDir) {
	    File dir = new File(destDir);
	    // create output directory if it doesn't exist
	    if(!dir.exists()) dir.mkdirs();
	    FileInputStream fis;
	    //buffer for read and write data to file
	    byte[] buffer = new byte[1024];
	    try {
	        fis = new FileInputStream(zipFilePath);
	        ZipInputStream zis = new ZipInputStream(fis);
	        ZipEntry ze = zis.getNextEntry();
	        while(ze != null){
	            String fileName = ze.getName();
	            File newFile = new File(destDir + File.separator + fileName);
	            System.out.println("Unzipping to "+newFile.getAbsolutePath());
	            //create directories for sub directories in zip
	            new File(newFile.getParent()).mkdirs();
	            FileOutputStream fos = new FileOutputStream(newFile);
	            int len;
	            while ((len = zis.read(buffer)) > 0) {
	            fos.write(buffer, 0, len);
	            }
	            fos.close();
	            //close this ZipEntry
	            zis.closeEntry();
	            ze = zis.getNextEntry();
	        }
	        //close last ZipEntry
	        zis.closeEntry();
	        zis.close();
	        fis.close();
	    } catch (IOException e) {
	        e.printStackTrace();
	    }
	    
	}
  
  private void setCurStartAndEndDateTime(String curStartTime, String curEndTime, String scenario) {
	  
		 if(ndParam.getTimePeriod() != null) {
		  if(ndParam.getTimePeriod().equals("Absolute Time")) {
			  ndParam.setCurStartTime(ndParam.getCurStartTimeAbsolute().replace(" ", "@"));
			  ndParam.setCurEndTime(ndParam.getCurEndTimeAbsolute().replace(" ", "@"));
		  } else if(ndParam.getTimePeriod().equals("Elapsed Time")) {
			  long startTime =  convertFormattedDateToMilliscond(curStartTime, "MM/dd/yy HH:mm:ss", null);
			  long elapsedStartTime = convertFormattedTimeToMillisecond(ndParam.getCurStartTimeElapsed(), ":");
			  long elapStartTime = startTime + elapsedStartTime;
			  String startElapsedTime = convertDateTimeStampToFormattedString(elapStartTime, "MM/dd/yy HH:mm:ss", null);
			 
			 // long endTime =  convertFormattedDateToMilliscond(curEndTime, "MM/dd/yy HH:mm:ss", null);
			  long elapsedEndTime = convertFormattedTimeToMillisecond(ndParam.getCurEndTimeElapsed(), ":");
			  long elapEndTime = startTime + elapsedEndTime;
			  String endElapsedTime = convertDateTimeStampToFormattedString(elapEndTime, "MM/dd/yy HH:mm:ss", null);
			  ndParam.setCurStartTime(startElapsedTime.replace(" ", "@"));
			  ndParam.setCurEndTime(endElapsedTime.replace(" ", "@"));
		  } else if(ndParam.getTimePeriod().equals("Phase")) {
			  
			  try {
			  
			  URL url;
		        String str = getUrlString(nsUrlConnectionString); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
		        url = new URL(str + "ProductUI/productSummary/jenkinsService/getPhaseDuration?phaseName=" + ndParam.getPhase() + "&testRun=" + testRun + "&scenarioName=" + scenarioName);
		        
		        HttpURLConnection conn = (HttpURLConnection) url.openConnection();
		        conn.setRequestMethod("GET");

		        conn.setRequestProperty("Accept", "application/json");

	        	if (conn.getResponseCode() != 200) {
	        	  logger.log(Level.INFO, "Getting Error code on polling  = " + conn.getResponseCode() + ". Retrying in next poll in 5 minutes.");
	        	}

	        	BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));
	        	String phaseResponse = br.readLine();

		    
		        JSONObject phaseDuration = (JSONObject) JSONSerializer.toJSON(phaseResponse);
		      	  
	      	 
	      	  
	      	    String startTime = phaseDuration.getString("start Time");
	      	    String endTime = phaseDuration.getString("end Time");
	      	    /*Terminating Loop when test is stopped.*/
	      	  
	      	    long startTimee =  convertFormattedDateToMilliscond(curStartTime, "MM/dd/yy HH:mm:ss", null);
	      	    long timeInMilisecond = convertFormattedTimeToMillisecond(startTime, ":");
	      	    long stTime = startTimee + timeInMilisecond;
	      	    String startElapsedTime = convertDateTimeStampToFormattedString(stTime, "MM/dd/yy HH:mm:ss", null);
	      	    
	      	    long endTimee =  convertFormattedDateToMilliscond(curEndTime, "MM/dd/yy HH:mm:ss", null);
	    	    long endtimeInMilisecond = convertFormattedTimeToMillisecond(endTime, ":");
	    	    long edTime = startTimee + endtimeInMilisecond;
	    	    String edElapsedTime = convertDateTimeStampToFormattedString(edTime, "MM/dd/yy HH:mm:ss", null);
	      	    ndParam.setCurStartTime(startElapsedTime.replace(" ", "@"));
			    ndParam.setCurEndTime(edElapsedTime.replace(" ", "@"));
		        
			  } catch(Exception e) {
				  logger.log(Level.SEVERE, "Unknown exception in establishing connection with Netstorm .", e);
			  }
			  } else if(ndParam.getTimePeriod().equals("Whole Scenario")) {
				  ndParam.setCurStartTime(curStartTime.replace(" ", "@"));
				  ndParam.setCurEndTime(curEndTime.replace(" ", "@"));
			  }
		  
		  setCurStart(curStartTime);
		  setCurEnd(curEndTime);
		  
		  logger.log(Level.INFO, "current stat time and end time...." + ndParam.getCurStartTime() + "   " + ndParam.getCurEndTime());
		 }
	       
	  }
	  
	  public static String convertDateTimeStampToFormattedString(long dateTimeStamp, String dateFormat, TimeZone timeZone)
	  {
	    try
	    {
	      SimpleDateFormat trDateFormat = new SimpleDateFormat(dateFormat);
	      
	      //If TimeZone is null then it use default.
	      if(timeZone != null)
	        trDateFormat.setTimeZone(timeZone);
	      
	      Date userDate = new Date(dateTimeStamp);
	      String formattedDate = trDateFormat.format(userDate);
	      return formattedDate;
	    }
	    catch(Exception e)
	    {
	      return "";
	    }
	  }
	  
	  public static long convertFormattedDateToMilliscond(String dateTime, String dateFormat, TimeZone timeZone)
	  {
	    try
	    {
	      SimpleDateFormat trDateFormat = new SimpleDateFormat(dateFormat);
	        
	      //If TimeZone is null then it use default.
	      if(timeZone != null)
	        trDateFormat.setTimeZone(timeZone);
	      
	      Date userDate = trDateFormat.parse(dateTime); 
	      long absoluteDateInMillies = userDate.getTime();	 
	      return absoluteDateInMillies;	   
	    }
	    catch(Exception e)
	    {
	      return 0L;
	    }
	  }
	  
	  public static long convertFormattedTimeToMillisecond(String formattedString, String seperater)
	  {
	    try
	    {
	      String arrTime[] = formattedString.split(seperater);
	      
	      //Validation for checking if time array size is not equals to 3, then return from here.
	      if(arrTime == null || arrTime.length != 3)
	      {
		return 0L;
	      }
	      
	      //First calculate in seconds.
	      int hourInSecond = Integer.parseInt(arrTime[0]) * 3600;
	      int minuteInSecond = Integer.parseInt(arrTime[1]) * 60;
	      int second = Integer.parseInt(arrTime[2]);
	      
	      long totalMilliscond = (hourInSecond + minuteInSecond + second) * 1000L;
	      
	      return totalMilliscond;
	    }
	    catch(Exception e)
	    {
	      return 0L;
	    }
	  }

  /*Method to parse json for both netstorm and netdiagnostic*/
  private MetricDataContainer parseJSONDataND(JSONObject resonseObj, JSONObject resonseObjND, String testMode) {
    logger.log(Level.INFO, "parseJSONData() called.");

    MetricDataContainer metricDataContainer = new MetricDataContainer();
    logger.log(Level.INFO, "Recived response from netstorm :" + resonseObj);
    logger.log(Level.INFO, "Recived response from netdiagnostic : " + resonseObjND);
    try {
      ArrayList<MetricData> dataList = new ArrayList<MetricData>();
      JSONObject jsonGraphs = (JSONObject) resonseObj.get("graphs");
      logger.log(Level.INFO, "Recived response from graphs for netstorm : " + jsonGraphs);
      int freq = ((Integer) resonseObj.get("frequency")) / 1000;
      logger.log(Level.INFO, "Recived response from netstorm  : frq = " + freq);
      metricDataContainer.setFrequency(freq);

      /*For netstorm json parsing */
      TestReport testReportNS = new TestReport();
      if ("T".equals(testMode)) {

        testReportNS = new TestReport();
        JSONObject jsonTestReportWholeObj = resonseObj.getJSONObject("testReport");
        logger.log(Level.INFO, "Recived response from whole report : " + jsonTestReportWholeObj);
        JSONObject jsonTestReport = jsonTestReportWholeObj.getJSONObject("members");

        logger.log(Level.INFO, "Recived response from test : " + jsonTestReport);
        String overAllStatus = jsonTestReport.getString("Overall Status");
        String date = jsonTestReport.getString("Date");
        String overAllFailCriteria = jsonTestReport.getString("Overall Fail Criteria (greater than red) %");
        //JSONArray metricsUnderTest = jsonTestReport.getJSONArray("Metrics Under Test");
        String serverName = jsonTestReport.getString("IP");
        String productName = jsonTestReport.getString("ProductName");
        String previousTestRun = jsonTestReport.getString("Previous Test Run");
        String baseLineTestRun = jsonTestReport.getString("Baseline Test Run");
        String initialTestRun = jsonTestReport.getString("Initial Test Run");
        String baseLineDateTime = jsonTestReport.getString("Baseline Date Time");
        String previousDateTime = jsonTestReport.getString("Previous Date Time");
        String initialDateTime = jsonTestReport.getString("Initial Date Time");
        String testRun = jsonTestReport.getString("Test Run");
        String normalThreshold = jsonTestReport.getString("Normal Threshold");
        String criticalThreshold = jsonTestReport.getString("Critical Threshold");
        String currentDateTime = "", previousDescription = "", baselineDescription = "", currentDescription = "", initialDescription = "";
        String dashboardURL = jsonTestReport.getString("Dashboard Link");
        String reportLink = jsonTestReport.getString("Report Link");
        
        try {
          currentDateTime = jsonTestReport.getString("Current Date Time");
          previousDescription = jsonTestReport.getString("Previous Description");
          baselineDescription = jsonTestReport.getString("Baseline Description");
          currentDescription = jsonTestReport.getString("Current Description");
          initialDescription = jsonTestReport.getString("Initial Description");
        } catch (Exception ex) {
          logger.log(Level.SEVERE, "NSNDIntegration - Error in parsing Test Report Data from  netstorm json:" + ex);
          logger.log(Level.SEVERE, "NSNDIntegration ---" + ex.getMessage());
        }
       if(jsonTestReport.get("Metrics Under Test") != null) {
         JSONArray metricsUnderTest = jsonTestReport.getJSONArray("Metrics Under Test");
        ArrayList<TestMetrics> testMetricsList = new ArrayList<TestMetrics>(metricsUnderTest.size());
        String str = "";
        int index = 0;
        for (Object jsonData : metricsUnderTest) {
          JSONObject jsonObject = (JSONObject) jsonData;

          String prevTestValue = jsonObject.getString("Prev Test Value ");
          String baseLineValue = jsonObject.getString("Baseline Value ");
          String initialValue = jsonObject.getString("Initial Value ");
          String edLink = jsonObject.getString("link");
          String currValue = jsonObject.getString("Value");
          String metric = jsonObject.getString("Metric");
          String metricRule = jsonObject.getString("MetricRule");
          String operator = jsonObject.getString("Operator");
          String sla = jsonObject.getString("SLA");
          if (sla.indexOf(">") != -1 || sla.indexOf(">") > 0) {
            sla = sla.substring(sla.lastIndexOf(">") + 1, sla.length());
          }
          
          String count = jsonObject.getString("Count");

          String transactiontStatus = jsonObject.getString("Transaction Status");
          String transactionBgcolor = jsonObject.getString("Transaction BGcolor");
          String transactionTooltip = jsonObject.getString("Transaction Tooltip");
          String trendLink = jsonObject.getString("trendLink");
          String metricLink = jsonObject.getString("Metric_DashboardLink");
          TestMetrics testMetric = new TestMetrics();

          testMetric.setBaseLineValue(baseLineValue);
          testMetric.setCurrValue(currValue);
          if (edLink != null) {
            testMetric.setEdLink(edLink);
          } else {
            testMetric.setEdLink("NA");
          }
          testMetric.setOperator(operator);
          testMetric.setPrevTestRunValue(prevTestValue);
          testMetric.setInitialValue(initialValue);
          testMetric.setSLA(sla);
          
          if(count.equals("noCount")){
        	  testMetric.setCount("NA");
          }
          else {
        	  testMetric.setCount(count);  
          }
          
          if (trendLink != null) {
            testMetric.setLinkForTrend(trendLink);
          } else {
            testMetric.setLinkForTrend("NA");
          }

          int fromPattern=0;
    	  String patt = jsonObject.getString("PATTERN");
//          String arrMetricDisplayName = arrMetricValue[0];
          if (metric.trim().contains("- PATTERN"))
          {
            int len = patt.length()+2;
    	        String dName = metric.substring(metric.indexOf("-")+len+1,metric.trim().length()-1);
    	        metric = metric.replace(metric.substring(metric.indexOf("-"), metric.length()),"- " + dName);
    	        fromPattern=1;
          }
          
          String headerName = "";
          String displayName = metric;
          if (index == 0) {
            str = displayName;
            if (displayName.contains("- All") && fromPattern!=1) {
              headerName = displayName.substring(0, str.lastIndexOf("-") + 5);
              displayName = displayName.substring(displayName.lastIndexOf("-") + 6, displayName.length() - 1);
            } else if (displayName.contains(" - ")) {
              headerName = displayName.substring(0, str.lastIndexOf("-") + 1);
              displayName = displayName.substring(displayName.lastIndexOf("-") + 1, displayName.length() - 1);
            } else {
              headerName = "Other";
            }
            index++;
          } else {
            if (displayName.contains(" - ") && (str.lastIndexOf("-")) != -1 ) {
              String metricName = displayName.substring(0, displayName.lastIndexOf("-"));

              if  (metricName.toString().trim().equals(str.substring(0, str.lastIndexOf("-")).toString().trim())) {
                headerName = "";
                if (displayName.contains("- All") && fromPattern!=1) {
                  displayName = displayName.substring(displayName.lastIndexOf("-") + 6, displayName.length() - 1);
                } else {
                  displayName = displayName.substring(displayName.lastIndexOf("-") + 1, displayName.length());
                }
              } else {
                str = displayName;
                if (displayName.contains("- All") && fromPattern!=1) {
                  headerName = displayName.substring(0, displayName.lastIndexOf("-") + 5);
                  displayName = displayName.substring(displayName.lastIndexOf("-") + 6, displayName.length() - 1);
                } else if (displayName.contains(" - ")) {
                  headerName = displayName.substring(0, displayName.lastIndexOf("-"));
                  displayName = displayName.substring(displayName.lastIndexOf("-") + 1, displayName.length());
                } else {
                  headerName = "Other";
                }

              }
            } else if (str.lastIndexOf("-") == -1) {
              str = displayName;

              if (displayName.contains("- All") && fromPattern!=1) {
                headerName = displayName.substring(0, str.lastIndexOf("-") + 5);
                displayName = displayName.substring(str.lastIndexOf("-") + 6, displayName.length() - 1);

              } else if (displayName.contains(" - ")) {
                headerName = displayName.substring(0, str.lastIndexOf("-"));
                displayName = displayName.substring(str.lastIndexOf("-") + 1, displayName.length());
              } else {
                headerName = "Other";
              }
            } else {
              headerName = "Other";
            }
          }
          
          testMetric.setNewReport("NewReport");
          testMetric.setDisplayName(displayName);
          testMetric.setHeaderName(headerName);
          testMetric.setMetricName(metric);
          testMetric.setMetricRuleName(metricRule);
          testMetric.setMetricLink(metricLink);
          testMetric.setTransactiontStatus(transactiontStatus);
          testMetric.setStatusColour(transactionBgcolor);
          testMetric.setTransactionTooltip(transactionTooltip);

          testMetricsList.add(testMetric);
          testReportNS.setOperator(operator);
          testReportNS.setTestMetrics(testMetricsList);
          if(count.equals("noCount")){
        	  testReportNS.setShowCount("0");
           }else {
        	   testReportNS.setShowCount("1");
           }
          
        }
       } else {
    	   if(jsonTestReport.get("Transaction Stats") != null) {
       		JSONObject transStats = (JSONObject)jsonTestReport.get("Transaction Stats");
       		testReportNS = metricDataForTrans(transStats, testReportNS, jsonTestReport);
       	   }
       	   if(jsonTestReport.get("Vector Groups") != null) {
       		JSONObject vectorGroups = (JSONObject)jsonTestReport.get("Vector Groups");
       		testReportNS = metricDataForVectorGroups(vectorGroups, testReportNS, jsonTestReport);
       	   }
       	   if(jsonTestReport.get("Scalar Groups") != null) {
       		JSONObject scalarGroups = (JSONObject)jsonTestReport.get("Scalar Groups");
       		testReportNS = metricDataForScalar(scalarGroups, testReportNS, jsonTestReport);
       	   }
       }

        int transObj = 1;

        //Check is used for if Base transaction exist in json report.
        if (jsonTestReport.has("BASETOT")) {
          transObj = 2;
        }

        for (int i = 0; i < transObj; i++) {
          JSONObject transactionJson = null;

          if (i == 1) {
            transactionJson = jsonTestReport.getJSONObject("BASETOT");
          } else {
            if (jsonTestReport.has("TOT")) {
              transactionJson = jsonTestReport.getJSONObject("TOT");
            } else {
              transactionJson = jsonTestReport.getJSONObject("CTOT");
            }
          }

          String complete = "NA";
          if (transactionJson.getString("complete") != null) {
            complete = transactionJson.getString("complete");
          }

          String totalTimeOut = "NA";
          if (transactionJson.getString("Time Out") != null) {
            totalTimeOut = transactionJson.getString("Time Out");
          }

          String t4xx = "NA";
          if (transactionJson.getString("4xx") != null) {
            t4xx = transactionJson.getString("4xx");
          }

          String t5xx = "NA";
          if (transactionJson.getString("5xx") != null) {
            t5xx = transactionJson.getString("5xx");
          }

          String conFail = "NA";
          if (transactionJson.getString("ConFail") != null) {
            conFail = transactionJson.getString("ConFail");
          }

          String cvFail = "NA";
          if (transactionJson.getString("C.V Fail") != null) {
            cvFail = transactionJson.getString("C.V Fail");
          }

          String success = "NA";
          if (transactionJson.getString("success") != null) {
            success = transactionJson.getString("success");
          }

          String warVersionTrans = "NA";
          if (transactionJson.has("warVersion")) {
            warVersionTrans = transactionJson.getString("warVersion");
          }

          String releaseVersionTrans = "NA";
          if (transactionJson.has("releaseVersion")) {
            releaseVersionTrans = transactionJson.getString("releaseVersion");
          }

          //Create Transaction Stats Object to save Base Test and Current Test Run transaction details
          TransactionStats transactionStats = new TransactionStats();
          if (i == 1) {
            transactionStats.setTransTestRun("BASETOT");
          } else {
            if (jsonTestReport.has("TOT")) {
              transactionStats.setTransTestRun("TOT");
            } else {
              transactionStats.setTransTestRun("CTOT");
            }
          }

          transactionStats.setComplete(complete);
          transactionStats.setConFail(conFail);
          transactionStats.setCvFail(cvFail);
          transactionStats.setSuccess(success);
          transactionStats.setT4xx(t4xx);
          transactionStats.setT5xx(t5xx);
          transactionStats.setTotalTimeOut(totalTimeOut);
          transactionStats.setWarVersion(warVersionTrans);
          transactionStats.setReleaseVersion(releaseVersionTrans);

          testReportNS.getTransStatsList().add(transactionStats);
        }

        testReportNS.setDashboardURL(dashboardURL);
        testReportNS.setReportLink(reportLink);
        testReportNS.setBaseLineTestRun(baseLineTestRun);
        testReportNS.setInitialTestRun(initialTestRun);
        testReportNS.setBaselineDateTime(baseLineDateTime);
        testReportNS.setPreviousDateTime(previousDateTime);
        testReportNS.setInitialDateTime(initialDateTime);
        testReportNS.setOverAllFailCriteria(overAllFailCriteria);
        testReportNS.setDate(date);
       // testReportNS.setTestMetrics(testMetricsList);
        testReportNS.setOverAllStatus(overAllStatus);
        testReportNS.setServerName(serverName);
        testReportNS.setIpPortLabel(productName);
        testReportNS.setPreviousTestRun(previousTestRun);
        testReportNS.setTestRun(testRun);
        testReportNS.setNormalThreshold(normalThreshold);
        testReportNS.setCriticalThreshold(criticalThreshold);
        testReportNS.setCurrentDateTime(currentDateTime);
        testReportNS.setPreviousDescription(previousDescription);
        testReportNS.setBaselineDescription(baselineDescription);
        testReportNS.setInitialDescription(initialDescription);
        testReportNS.setCurrentDescription(currentDescription);
        metricDataContainer.setTestReport(testReportNS);
      }

      TestReport testReportND = new TestReport();
      //For ND report
      
       String prefix[] ;
       prefix = ndUrlConnectionString.split(("//"));
      if ("T".equals(testMode)) {

        testReportND = new TestReport();
        JSONObject jsonTestReportWholeObj = resonseObjND.getJSONObject("testReport");
        logger.log(Level.INFO, "Recived response from whole report in  case of ND : " + jsonTestReportWholeObj);
        JSONObject jsonTestReport = jsonTestReportWholeObj.getJSONObject("members");

        logger.log(Level.INFO, "Recived response from test report in  case of ND : " + jsonTestReport);
        String overAllStatus = jsonTestReport.getString("Overall Status");
        String date = jsonTestReport.getString("Date");
        String overAllFailCriteria = jsonTestReport.getString("Overall Fail Criteria (greater than red) %");
     //   JSONArray metricsUnderTest = jsonTestReport.getJSONArray("Metrics Under Test");
        String serverName = jsonTestReport.getString("IP");
        String productName = jsonTestReport.getString("ProductName");
        String previousTestRun = jsonTestReport.getString("Previous Test Run");
        String baseLineTestRun = jsonTestReport.getString("Baseline Test Run");
        String initialTestRun = jsonTestReport.getString("Initial Test Run");
        String baseLineDateTime = jsonTestReport.getString("Baseline Date Time");
        String previousDateTime = jsonTestReport.getString("Previous Date Time");
        String initialDateTime = jsonTestReport.getString("Initial Date Time");
        String testRun = jsonTestReport.getString("Test Run");
        String normalThreshold = jsonTestReport.getString("Normal Threshold");
        String criticalThreshold = jsonTestReport.getString("Critical Threshold");
        String dashboardURL = jsonTestReport.getString("Dashboard Link");
        String reportLink = jsonTestReport.getString("Report Link");
        String currentDateTime = "", previousDescription = "", baselineDescription = "", currentDescription = "", initialDescription = "";
        try {
          // currentDateTime = jsonTestReport.getString("Current Date Time");
          previousDescription = jsonTestReport.getString("Previous Description");
          baselineDescription = jsonTestReport.getString("Baseline Description");
          currentDescription = jsonTestReport.getString("Current Description");
          initialDescription = jsonTestReport.getString("Initial Description");
        } catch (Exception ex) {
          logger.log(Level.SEVERE, " NSNDIntegration -Error in parsing Test Report Data from netdiagnostics json :" + ex);
          logger.log(Level.SEVERE, "NSNDIntegration Error ---" + ex.getMessage());
        }

       if(jsonTestReport.get("Metrics Under Test") != null) {
    	 JSONArray metricsUnderTest = jsonTestReport.getJSONArray("Metrics Under Test");
        ArrayList<TestMetrics> testMetricsList = new ArrayList<TestMetrics>(metricsUnderTest.size());

        String str = "";
        int index = 0;
        for (Object jsonData : metricsUnderTest) {
          JSONObject jsonObject = (JSONObject) jsonData;

          String prevTestValue = jsonObject.getString("Prev Test Value ");
          String baseLineValue = jsonObject.getString("Baseline Value ");
          String initialValue = jsonObject.getString("Initial Value ");
          String edLink = jsonObject.getString("link");
          String currValue = jsonObject.getString("Value");
          String metric = jsonObject.getString("Metric");
          String metricRule = jsonObject.getString("MetricRule");
          String operator = jsonObject.getString("Operator");
          String sla = jsonObject.getString("SLA");
          if (sla.indexOf(">") != -1 || sla.indexOf(">") > 0) {
            sla = sla.substring(sla.lastIndexOf(">") + 1, sla.length());
          }

          String transactiontStatus = jsonObject.getString("Transaction Status");
          String transactionBgcolor = jsonObject.getString("Transaction BGcolor");
          String transactionTooltip = jsonObject.getString("Transaction Tooltip");
          String trendLink = jsonObject.getString("trendLink");
          String metricLink = jsonObject.getString("Metric_DashboardLink");
          
          String count = jsonObject.getString("Count");
          TestMetrics testMetric = new TestMetrics();

          if(count.equals("noCount")){
        	  testMetric.setCount("NA");
          }
          else {
        	  testMetric.setCount(count);  
          }
          testMetric.setBaseLineValue(baseLineValue);
          testMetric.setCurrValue(currValue);
          
          if (edLink != null) {
             testMetric.setEdLink(edLink);
          } else {
            testMetric.setEdLink("NA");
          }
          testMetric.setOperator(operator);
          testMetric.setPrevTestRunValue(prevTestValue);
          testMetric.setInitialValue(initialValue);
          testMetric.setSLA(sla);
//          
//          if (trendLink != null) {
//            
//           if(prefix[0] != "")
//            trendLink = prefix[0] + trendLink;
             
            testMetric.setLinkForTrend(trendLink);
//          } else {
//            testMetric.setLinkForTrend("NA");
//          }
//         if(ndParam.getBaseStartTime() != null && !ndParam.getBaseStartTime().equals("") && !ndParam.getBaseStartTime().equals("NA")) {
//          String[] startDateTime = ndParam.getBaseStartTime().spligetBase1EndTime     String[] endDateTime = ndParam.getBaseEndTime().split("@");
//
//          if (startDateTime[0].equals(endDateTime[0])) {
//            baseLineDateTime = ndParam.getBaseStartTime().replace("@", " ") + " to " + endDateTime[1];
//          } //baseLineDateTime = startDateTime[0] + " to "+ endDateTime[1];
//          else {
//            baseLineDateTime = ndParam.getBasgetBase1EndTimeeplace("@", " ") + " to " + ndParam.getBaseEndTime().replace("@", " ");
//          }
//         }
//
//         if (ndParam.getInitEndTime() != null) {
//             String[] initStartDateTime = ndParam.getInitStartTime().split("@");
//             String[] initEndDateTime = ndParam.getInitEndTime().split("@");
//             if (initStartDateTime[0].equals(initEndDateTime[0])) {
//               initialDateTime = ndParam.getInitStartTime().replace("@", " ") + " to " + initEndDateTime[1];
//             } else {
//               initialDateTime = ndParam.getInitStartTime().replace("@", " ") + " to " + ndParam.getInitEndTime().replace("@", " ");
//             }
//           }

         int fromPattern=0;
   	  String patt = jsonObject.getString("PATTERN");
//         String arrMetricDisplayName = arrMetricValue[0];
         if (metric.trim().contains("- PATTERN"))
         {
           int len = patt.length()+2;
   	        String dName = metric.substring(metric.indexOf("-")+len+1,metric.trim().length()-1);
   	        metric = metric.replace(metric.substring(metric.indexOf("-"), metric.length()),"- " + dName);
   	        fromPattern=1;
         }
   	  
   	  String headerName = "";
         String displayName = metric;
         if (index == 0)
         {
           str = displayName;
           if(displayName.contains("- All") && fromPattern!=1)
           {
              headerName = displayName.substring(0, str.indexOf("-")+5);
              displayName = displayName.substring(displayName.indexOf("-")+6,displayName.length()-1);
           }
           else if(displayName.contains(" - "))
           {
             headerName = displayName.substring(0, str.indexOf("-")+1);
             displayName = displayName.substring(displayName.indexOf("-")+1,displayName.length()-1);
           }
           else
           {
             headerName = "Other";
           }
             index++;
          }
          else
          {
            if (displayName.contains(" - ") && (str.indexOf("-")) != -1 )
            {
              String metricName = displayName.substring(0, displayName.indexOf("-"));

              if (metricName.toString().trim().equals(str.substring(0, str.indexOf("-")).toString().trim()))
              {
                 headerName = "";
                if (displayName.contains("- All") && fromPattern!=1)
                {
                  displayName = displayName.substring(displayName.indexOf("-")+6,displayName.length()-1);
                }
                else
                  displayName = displayName.substring(displayName.indexOf("-")+1,displayName.length());
               }
              else
              {
                str = displayName;
                if (displayName.contains("- All") && fromPattern!=1)
                {
                  headerName = displayName.substring(0, displayName.indexOf("-")+5);
                  displayName = displayName.substring(displayName.indexOf("-")+6,displayName.length()-1);
                }
                else if(displayName.contains(" - "))
                {
                  headerName = displayName.substring(0, displayName.indexOf("-"));
                  displayName = displayName.substring(displayName.indexOf("-")+1,displayName.length());
                }
                else
                {
                  headerName = "Other";
                }
                
              }
            }
            else if(str.indexOf("-") == -1)
            { 
              str = displayName;
              
              if(displayName.contains("- All") && fromPattern!=1)
              { 
                headerName = displayName.substring(0, str.indexOf("-")+5);
                displayName = displayName.substring(str.indexOf("-")+6,displayName.length()-1);
              
              }
              else if(displayName.contains(" - "))
              { 
                headerName = displayName.substring(0, str.indexOf("-"));
                displayName = displayName.substring(str.indexOf("-")+1,displayName.length());
              }
              else
              { 
                headerName = "Other";
              }
            }
            else
            {
             headerName = "Other";
            }
          }

         testMetric.setNewReport("NewReport");
         testMetric.setDisplayName(displayName);
         testMetric.setHeaderName(headerName);
          testMetric.setMetricName(metric);
          testMetric.setMetricRuleName(metricRule);
          testMetric.setTransactiontStatus(transactiontStatus);
          testMetric.setStatusColour(transactionBgcolor);
          testMetric.setTransactionTooltip(transactionTooltip);
          
//          if(prefix[0] != "")
//           metricLink = prefix[0] + metricLink;
          
          testMetric.setMetricLink(metricLink);
          testMetricsList.add(testMetric);
          testReportND.setOperator(operator);
          testReportND.setTestMetrics(testMetricsList);
        }
       } else {
    	   if(jsonTestReport.get("Transaction Stats") != null) {
       		JSONObject transStats = (JSONObject)jsonTestReport.get("Transaction Stats");
       		testReportND = metricDataForTrans(transStats, testReportND, jsonTestReport);
       	   }
       	   if(jsonTestReport.get("Vector Groups") != null) {
       		JSONObject vectorGroups = (JSONObject)jsonTestReport.get("Vector Groups");
       		testReportND = metricDataForVectorGroups(vectorGroups, testReportND, jsonTestReport);
       	   }
       	   if(jsonTestReport.get("Scalar Groups") != null) {
       		JSONObject scalarGroups = (JSONObject)jsonTestReport.get("Scalar Groups");
       		testReportND = metricDataForScalar(scalarGroups, testReportND, jsonTestReport);
       	   }
       }

        int transObj = 1;

        //Check is used for if Base transaction exist in json report.
        if (jsonTestReport.has("BASETOT")) {
          transObj = 2;
        }

        testReportND.setShowHideTransaction(true);

//        if(prefix[0] != "")
//          dashboardURL = prefix[0]+dashboardURL;
  
        testReportND.setDashboardURL(dashboardURL);
        
//        if(prefix[0] != "")
//          reportLink = prefix[0]+reportLink;
        
        testReportND.setReportLink(reportLink);
        testReportND.setBaseLineTestRun(baseLineTestRun);
        testReportND.setInitialTestRun(initialTestRun);
        testReportND.setBaselineDateTime(baseLineDateTime);
        testReportND.setPreviousDateTime(previousDateTime);
        testReportND.setInitialDateTime(initialDateTime);
        testReportND.setOverAllFailCriteria(overAllFailCriteria);
        testReportND.setDate(date);
        //testReportND.setTestMetrics(testMetricsList);
        testReportND.setOverAllStatus(overAllStatus);
        testReportND.setServerName(serverName);
        testReportND.setIpPortLabel(productName);
        testReportND.setPreviousTestRun(previousTestRun);
        testReportND.setTestRun(testRun);
        testReportND.setNormalThreshold(normalThreshold);
        testReportND.setCriticalThreshold(criticalThreshold);
        testReportND.setCurrentDateTime(ndParam.getCurStartTime().replace("@", " ") + " to " + ndParam.getCurEndTime().replace("@", " "));
        testReportND.setPreviousDescription(previousDescription);
        testReportND.setBaselineDescription(baselineDescription);
        testReportND.setInitialDescription(initialDescription);
        testReportND.setCurrentDescription(currentDescription);
        metricDataContainer.setTestReportND(testReportND);

      }

      Set keySet = jsonGraphs.keySet();
      Iterator itr = keySet.iterator();

      while (itr.hasNext()) {
        String key = (String) itr.next();
        MetricData metricData = new MetricData();

        JSONObject graphJsonObj = (JSONObject) jsonGraphs.get(key);
        String graphName = (String) graphJsonObj.get("graphMetricPath");

        metricData.setMetricPath(graphName.substring(graphName.indexOf("|") + 1));
        metricData.setFrequency(String.valueOf(freq));

        JSONArray jsonArray = (JSONArray) graphJsonObj.get("graphMetricValues");

        ArrayList<MetricValues> list = new ArrayList<MetricValues>();

        for (Object jsonArray1 : jsonArray) {
          MetricValues values = new MetricValues();
          JSONObject graphValues = (JSONObject) jsonArray1;
          String currVal = String.valueOf(graphValues.get("currentValue"));
          String maxVal = String.valueOf(graphValues.get("maxValue"));
          String minVal = String.valueOf(graphValues.get("minValue"));
          String avg = String.valueOf(graphValues.get("avgValue"));
          long timeStamp = (Long) graphValues.get("timeStamp");
          values.setValue((Double) graphValues.get("currentValue"));
          values.setMax((Double) graphValues.get("maxValue"));
          values.setMin(getMinForMetric((Double) graphValues.get("minValue")));
          values.setStartTimeInMillis(timeStamp);
          list.add(values);
        }

        metricData.setMetricValues(list);
        dataList.add(metricData);
        metricDataContainer.setMetricDataList(dataList);
      }

      //Now checking in response for baseline and previous test data
      if (testMode.equals("T")) {
        if (resonseObj.get("previousTestDataMap") != null) {
          JSONObject jsonGraphObj = (JSONObject) resonseObj.get("previousTestDataMap");

          ArrayList<MetricData> prevMetricDataList = parsePreviousAndBaseLineData(jsonGraphObj, freq, "Previous Test Run");

          if ((prevMetricDataList != null)) {
            logger.log(Level.INFO, "Setting previous test data in metric container = " + prevMetricDataList);
            metricDataContainer.setMetricPreviousDataList(prevMetricDataList);
          }
        }

        if (resonseObj.get("baseLineTestDataMap") != null) {
          JSONObject jsonGraphObj = (JSONObject) resonseObj.get("baseLineTestDataMap");
          ArrayList<MetricData> baseLineMetricDataList = parsePreviousAndBaseLineData(jsonGraphObj, freq, "Base Line Test Run");

          if ((baseLineMetricDataList != null)) {
            logger.log(Level.INFO, "Setting baseline test data in metric container = " + baseLineMetricDataList);
            metricDataContainer.setMetricBaseLineDataList(baseLineMetricDataList);
          }
        }
      }
    } catch (Exception e) {
      logger.log(Level.SEVERE, "NSNDIntegration - Error in parsing metrics stats");
      logger.log(Level.SEVERE, "NSNDIntegration - Metric Data:" + e);
      logger.log(Level.SEVERE, "NSNDIntegration ---" + e.getMessage());
      return null;
    }

    logger.log(Level.INFO, "NSNDIntegration - Metric Data:" + metricDataContainer);
    return metricDataContainer;
  }

  private TestReport metricDataForVectorGroups(JSONObject vectorGroups, TestReport testReport, JSONObject jsonTestReport) {
	  try {

		  logger.log(Level.INFO, "metricDataForVectorGroups() called.");
		  
		  /* getting baseline, previous and initial testRuns from testReport JSONObject */
		  String previousTestRun = jsonTestReport.getString("Previous Test Run");
		  String baseLineTestRun = jsonTestReport.getString("Baseline Test Run");
		  String initialTestRun = jsonTestReport.getString("Initial Test Run");

		  /* ArrayList of metric info type to set the final values in testReport object */
		  ArrayList<MetricInfo> arr = new ArrayList();
		  
		  /* to iterate the vectorList object and get the groupNames */
		  Iterator<String> keys = vectorGroups.keys();

		  /* iterating the group names */
		  while(keys.hasNext()) {
			  String groupName = keys.next();
			  MetricInfo info = new MetricInfo();

			  info.setGroupName(groupName);
			  JSONObject groupInfo = (JSONObject)vectorGroups.get(groupName);

			  /* getting Metric Name List,Vector List and Metric Info objects from JSON */
			  JSONArray metricListForVector = (JSONArray)groupInfo.get("MetricName");
			  JSONArray vectorListForVector = (JSONArray)groupInfo.get("vector List");
			  JSONObject metricInfoForVector = (JSONObject)groupInfo.get("Metric Info");
			  JSONObject linkInfo = (JSONObject) groupInfo.get("link");
			  
			  /* ArrayList for setting the vectorList in metricInfo */
			  /* metric info class consists of getter/setters for groupName, info of group (i.e.data of table),vectorList and vectorObj */
			  /* vectorObj is of type metricval */
			  ArrayList<String> vectorList = new ArrayList();
			  ArrayList<ScalarVal> arrForVectorinfo = new ArrayList();


			  for(int i=0;i<vectorListForVector.size();i++) {
				  vectorList.add((String)vectorListForVector.get(i));
			  }
			  info.setVectorList(vectorList);

			  ArrayList<MetricLinkInfo> metricLinkInfo = new ArrayList<MetricLinkInfo>();
			  /* setting values of data for vector groups table */
			  for(int i=0;i<vectorListForVector.size();i++) {
				  String vectorNameVec = (String)vectorListForVector.get(i);
				  logger.log(Level.INFO, "vector name--"+vectorNameVec);

				  for(int j=0;j<metricListForVector.size();j++) {
					  String metricNameVec = (String)metricListForVector.get(j);
					  logger.log(Level.INFO, "metric name--"+metricNameVec);

					  if(metricInfoForVector.containsKey(vectorNameVec)) {
						  JSONArray vectorInfo = (JSONArray)metricInfoForVector.get(vectorNameVec);

						  for(int k=0;k<vectorInfo.size();k++) {
							  JSONObject metricVecInfo = (JSONObject)vectorInfo.get(k);

							  if(metricVecInfo.containsKey(metricNameVec)) {
								  JSONObject metricInfoFinal = (JSONObject)metricVecInfo.get(metricNameVec);

								  ScalarVal finalInfoForVector = new ScalarVal();

								  String Op = (String)metricInfoFinal.get("Operator");
								  String prev = (String)metricInfoFinal.get("Prev Test Value "); 
								  String Product = (String)metricInfoFinal.get("Prod");
								  String baselineValue = (String)metricInfoFinal.get("Baseline Value "); 
								  String transStatus = (String)metricInfoFinal.get("Transaction Status");
								  String transTooltip = (String)metricInfoFinal.get("Transaction Tooltip");
								  String transBGcolor = (String)metricInfoFinal.get("Transaction BGcolor");
								  String Value = (String)metricInfoFinal.get("Value");
								  String linkss = (String)metricInfoFinal.get("link");
								  String trendLink = (String)metricInfoFinal.get("Trend Link");
								  String SLA = (String)metricInfoFinal.get("SLA");
								  String initialValue = (String)metricInfoFinal.get("Initial Value "); 
								  String Stress = (String)metricInfoFinal.get("Stress");



								  finalInfoForVector.setOperator(Op);
								  finalInfoForVector.setPrevTestValue(prev);
								  finalInfoForVector.setProd(Product);
								  finalInfoForVector.setBaselineValue(baselineValue);
								  finalInfoForVector.setTransactionStatus(transStatus);
								  finalInfoForVector.setTransactionTooltip(transTooltip);
								  finalInfoForVector.setTransactionBGcolor(transBGcolor);
								  finalInfoForVector.setValue(Value);
								  finalInfoForVector.setLink(linkss);
								  finalInfoForVector.setSLA(SLA);
								  finalInfoForVector.setInitialValue(initialValue);
								  finalInfoForVector.setStress(Stress);
								  finalInfoForVector.setMetricName(metricNameVec);
								  finalInfoForVector.setVectorName(vectorNameVec);
								  finalInfoForVector.setTrendLink(trendLink);

								  arrForVectorinfo.add(finalInfoForVector);


							  }
						  }
					  }

				  }
				  MetricLinkInfo link = new MetricLinkInfo();
				  link.setVectorName(vectorNameVec);
				  String lnk = (String) linkInfo.get(vectorNameVec);
				  link.setLink(lnk);
				  metricLinkInfo.add(link);
			  }

			  info.setMetricLink(metricLinkInfo);
			  /* setting table data from ScalarVal object in metricInfo object */
			  info.setGroupInfo(arrForVectorinfo);

			  ArrayList<MetricVal> vectorArrFinal = new ArrayList();

			  String vectorVal = vectorList.get(0);

			  /* setting headers for vector groups table */
			  for(int i=0;i<metricListForVector.size();i++) {
				  String metrcNames = (String)metricListForVector.get(i);
				  int counts = 0;

				  MetricVal metrVal = new MetricVal();

				  JSONArray metricArrSec = (JSONArray)metricInfoForVector.get(vectorVal);

				  for(int j=0;j<metricArrSec.size();j++) {
					  JSONObject metricObj = (JSONObject)metricArrSec.get(j);

					  if(metricObj.containsKey(metrcNames)) {
						  JSONObject finalMetric = (JSONObject)metricObj.get(metrcNames);
						  ArrayList<String> headerForVector = new ArrayList();

						  headerForVector.add("SLA");
						  String pr = (String)finalMetric.get("Prod");
						  if(!pr.equals("0.0") && !pr.equals("-")) {
							  counts = counts+1;
							  headerForVector.add("Prod");
							  metrVal.setProd(true);
						  }

						  String st = (String)finalMetric.get("Stress");
						  if(!st.equals("0.0") && !st.equals("-")) {
							  counts = counts+1;
							  headerForVector.add("Stress");
							  metrVal.setStress(true);
						  }
						  if(!baseLineTestRun.equals("-1")) {
							  counts = counts+1;
							  headerForVector.add("Baseline TR");
						  }
						  if(!initialTestRun.equals("-1")) {
							  counts = counts+1;
							  headerForVector.add("Initial TR");
						  }
						  if(!previousTestRun.equals("-1")) {
							  counts = counts+1;
							  headerForVector.add("Previous TR");
						  }
						  String trans = (String)finalMetric.get("Transaction Status");
						  if(!trans.equals("-1")) {
							  counts = counts+1;
							  headerForVector.add("Success (%)");
							  metrVal.setTrans(true);
						  }

						  counts = counts+1;

						  metrVal.setCountForBenchmark(counts);
						  int countForMetrices = counts+2;
						  metrVal.setCountForMetrices(countForMetrices);
						  metrVal.setHeadersForTrans(headerForVector);
						  metrVal.setNameOfMetric(metrcNames);




					  }
				  }
				  /* setting final metricVal object in ArrayList */
				  vectorArrFinal.add(metrVal);
			  }
			  info.setVectorObj(vectorArrFinal);
			  arr.add(info);

		  }
		  testReport.setVectorValues(arr);

		  return testReport;	  
	  } catch(Exception e) {
		  logger.log(Level.SEVERE, "Error in getting metric data for vector Group" );
		  logger.log(Level.SEVERE, "Metric Data:" + e);
		  logger.log(Level.SEVERE, "---" + e.getMessage());
		  return null;  
	  }
  }

  private TestReport metricDataForTrans(JSONObject transGroup, TestReport testReport, JSONObject jsonTestReport) {
	  try {

		  logger.log(Level.INFO, "metricDataForTrans() called.");
		  
		  /* getting baseline, previous and initial testRuns from testReport JSONObject */
		  String previousTestRun = jsonTestReport.getString("Previous Test Run");
		  String baseLineTestRun = jsonTestReport.getString("Baseline Test Run");
		  String initialTestRun = jsonTestReport.getString("Initial Test Run");

		  /* getting Metric Name List,Vector List and Metric Info objects from JSON */
		  JSONArray metricNamesForTrans = (JSONArray)transGroup.get("MetricName");
		  JSONArray vectorListForTrans = (JSONArray)transGroup.get("vector List");
		  JSONObject metricInfoForTrans = (JSONObject)transGroup.get("Metric Info");
		  JSONObject linkObj = (JSONObject) transGroup.get("link");
		  int prodCount = 0;
		  int stressCount = 0;
		  
		  /* ArrayList for setting the values in ScalarVal class*/
		  /* ScalarVal class consists of setter/getters for data of table */
		  ArrayList<ScalarVal> transArr = new ArrayList();
          
		  /* ArrayList for setting the vectorList in testReport */
		  ArrayList<String> vectorForTrans  = new ArrayList();

		  for(int i=0;i<vectorListForTrans.size();i++) {
			  vectorForTrans.add((String)vectorListForTrans.get(i));
		  }

		  testReport.setVecList(vectorForTrans);
		  
		  ArrayList<MetricLinkInfo> merticLink = new ArrayList<MetricLinkInfo>();
		  /* setting values of data for transaction stats table */
		  for(int i=0;i<vectorListForTrans.size();i++) {

			  String vectorNameTrans = (String)vectorListForTrans.get(i);

			  for(int j=0;j<metricNamesForTrans.size();j++) {
				  String metricNameTranss = (String)metricNamesForTrans.get(j);
				  ScalarVal transValue = new ScalarVal();
				  if(metricInfoForTrans.containsKey(vectorNameTrans)) {

					  JSONArray metricArr = (JSONArray)metricInfoForTrans.get(vectorNameTrans);
					  for(int k=0;k<metricArr.size();k++) {
						  JSONObject newMetric = (JSONObject)metricArr.get(k);

						  if(newMetric.containsKey(metricNameTranss)) {
							  JSONObject metricObj = (JSONObject)newMetric.get(metricNameTranss);

							  String Oper = (String)metricObj.get("Operator");
							  String prevTest = (String)metricObj.get("Prev Test Value ");
							  String Production = (String)metricObj.get("Prod");
							  String baselineVal = (String)metricObj.get("Baseline Value ");
							  String transStatus = (String)metricObj.get("Transaction Status");
							  String transactionTool = (String)metricObj.get("Transaction Tooltip");
							  String transactionBG = (String)metricObj.get("Transaction BGcolor");
							  String Val = (String)metricObj.get("Value");
							  String links = (String)metricObj.get("link");
							  String sla = (String)metricObj.get("SLA");
							  String initialVal = (String)metricObj.get("Initial Value "); 
							  String stress = (String)metricObj.get("Stress");
							  String trendLink = (String)metricObj.get("Trend Link");

							  transValue.setOperator(Oper);
							  transValue.setPrevTestValue(prevTest);
							  transValue.setProd(Production);
							  transValue.setBaselineValue(baselineVal);
							  transValue.setTransactionStatus(transStatus);
							  transValue.setTransactionTooltip(transactionTool);
							  transValue.setTransactionBGcolor(transactionBG);
							  transValue.setValue(Val);
							  transValue.setLink(links);
							  transValue.setSLA(sla);
							  transValue.setInitialValue(initialVal);
							  transValue.setStress(stress);
							  transValue.setMetricName(metricNameTranss);
							  transValue.setVectorName(vectorNameTrans);
							  transValue.setTrendLink(trendLink);

						  }
					  }

				  }
				  transArr.add(transValue);
			  }
			  MetricLinkInfo linkInfo = new MetricLinkInfo();
              linkInfo.setVectorName(vectorNameTrans);
              String link = (String) linkObj.get(vectorNameTrans);
              linkInfo.setLink(link);
              merticLink.add(linkInfo);
		  }
		  /* setting table data from ScalarVal object in testReport object */
		  testReport.setTransactionStats(transArr);
		  testReport.setTransMetricLink(merticLink);

		  ArrayList<MetricVal> metricArrFinal = new ArrayList();

		  String vectorAny = vectorForTrans.get(0);

		  for (int i=0;i<metricNamesForTrans.size();i++) {
			  String metrcNames = (String)metricNamesForTrans.get(i);
			  int count = 0;

			  /* for setting metricName, count for benchmark , count for metrices and headers for table  */
			  MetricVal metrVal = new MetricVal();

			  JSONArray metricArrSec = (JSONArray)metricInfoForTrans.get(vectorAny);

			  /* setting headers for transaction stats table */
			  for(int j=0;j<metricArrSec.size();j++) {
				  JSONObject metricObj = (JSONObject)metricArrSec.get(j);

				  if(metricObj.containsKey(metrcNames)) {
					  JSONObject finalMetric = (JSONObject)metricObj.get(metrcNames);
					  ArrayList<String> transHeader = new ArrayList();
					  transHeader.add("SLA");
					  String prod = (String) finalMetric.get("Prod");
					  if(!prod.equals("0.0") && !prod.equals("-")) {
						  count = count+1;
						  transHeader.add("Prod");
						  metrVal.setProd(true);
					  }
					  String stress = (String) finalMetric.get("Stress");
					  if(!stress.equals("0.0") && !stress.equals("-")) {
						  count = count+1;
						  transHeader.add("Stress");
						  metrVal.setStress(true);
					  }
					  if(!baseLineTestRun.equals("-1")) {
						  count = count+1;
						  transHeader.add("Baseline TR");
					  }
					  if(!initialTestRun.equals("-1")) {
						  count = count+1;
						  transHeader.add("Initial TR");
					  }
					  if(!previousTestRun.equals("-1")) {
						  count = count+1;
						  transHeader.add("Previous TR");
					  }
					  String transactionStatus = (String)finalMetric.get("Transaction Status");
					  if(!transactionStatus.equals("-1")) {
						  count = count+1;
						  transHeader.add("Success (%)");
						  metrVal.setTrans(true);
					  }

					  count = count+1;

					  metrVal.setCountForBenchmark(count);
					  int countForMetrices = count+2;
					  metrVal.setCountForMetrices(countForMetrices);
					  metrVal.setHeadersForTrans(transHeader);
					  metrVal.setNameOfMetric(metrcNames); 
				  }

			  }
			  /* setting final metricVal object in ArrayList */
			  metricArrFinal.add(metrVal);
		  }
		  testReport.setMetricValues(metricArrFinal);

		  return testReport;  

	  } catch (Exception e) {
		  logger.log(Level.SEVERE, "Error in getting metric data" );
		  logger.log(Level.SEVERE, "Metric Data:" + e);
		  logger.log(Level.SEVERE, "---" + e.getMessage());
		  return null;  
	  }
  }

  private TestReport metricDataForScalar(JSONObject scalarGroups, TestReport testReport, JSONObject jsonTestReport) {
	  try {

		  logger.log(Level.INFO, "metricDataForScalar() called.");
		  
		  /* getting baseline, previous and initial testRuns from testReport JSONObject */
		  String previousTestRun = jsonTestReport.getString("Previous Test Run");
		  String baseLineTestRun = jsonTestReport.getString("Baseline Test Run");
		  String initialTestRun = jsonTestReport.getString("Initial Test Run");

		  /* getting Metric Name List and metricGroups info objects from JSON */
		  JSONArray metricNames = (JSONArray)scalarGroups.get("MetricName");
		  JSONObject metricGrp = (JSONObject)scalarGroups.get("Metric Info");
		  JSONObject metricLink = (JSONObject)scalarGroups.get("link");

		  /* ArrayList for setting the values in ScalarVal class*/
		  /* ScalarVal class consists of setter/getters for data of table */
		  ArrayList<ScalarVal> scalarArr = new ArrayList(); 
		  /* for counting the availability of prod value in every metric */
		  int countForProd = 0;
		  /* for counting the availability of stess value in every metric */
		  int countForStress = 0;
		  /* for counting the availability of transaction status(for success(%)) value in every metric */
		  int countForTrans = 0;
		  /* ArrayList for adding the headers for scalar groups table */
		  ArrayList<String> scalarHeader = new ArrayList();

		  /* setting values of data for scalar groups table */
		  for(int i=0;i<metricNames.size();i++) {

			  String name = (String)metricNames.get(i);
			  if(metricGrp.containsKey(name)) {
				  JSONObject scalarVal = (JSONObject)metricGrp.get(name);


				  String Operator = (String)scalarVal.get("Operator");
				  String preValue = (String)scalarVal.get("Prev Test Value ");
				  String Prod = (String)scalarVal.get("Prod");
				  String baselineValue = (String)scalarVal.get("Baseline Value "); 
				  String transactionStatus = (String)scalarVal.get("Transaction Status");
				  String transactionTooltip = (String)scalarVal.get("Transaction Tooltip");
				  String transactionBGcolor = (String)scalarVal.get("Transaction BGcolor");
				  String Value = (String)scalarVal.get("Value");
				  
				  String link = (String) metricLink.get(name);
				  String trendLink = (String)scalarVal.get("Trend Link");
				  
				  String SLA = (String)scalarVal.get("SLA");
				  String initialValue = (String)scalarVal.get("Initial Value "); 
				  String Stress = (String)scalarVal.get("Stress");
				  logger.log(Level.INFO, "Prev Test Value------ "+preValue);

				  if(!Prod.equals("0.0") && !Prod.equals("-1")) {
					  countForProd = countForProd+1;
				  }
				  if(!Stress.equals("0.0") && !Stress.equals("-1")) {
					  countForStress = countForStress+1;
				  }
				  if(!transactionStatus.equals("-1")) {
					  countForTrans = countForTrans+1;
				  }

				  ScalarVal scalarValue = new ScalarVal();

				  scalarValue.setOperator(Operator);
				  scalarValue.setPrevTestValue(preValue);
				  scalarValue.setProd(Prod);
				  scalarValue.setBaselineValue(baselineValue);
				  scalarValue.setTransactionStatus(transactionStatus);
				  scalarValue.setTransactionTooltip(transactionTooltip);
				  scalarValue.setTransactionBGcolor(transactionBGcolor);
				  scalarValue.setValue(Value);
				  scalarValue.setLink(link);
				  scalarValue.setSLA(SLA);
				  scalarValue.setInitialValue(initialValue);
				  scalarValue.setStress(Stress);
				  scalarValue.setMetricName(name);
				  scalarValue.setTrendLink(trendLink);

				  scalarArr.add(scalarValue);
			  }
		  }
		  testReport.setScalarGroups(scalarArr);
		  
		  /* setting headers for scalar groups table */
		  scalarHeader.add("SLA");
		  if(countForProd != 0) {
			  scalarHeader.add("Prod");
		  }

		  if(countForStress != 0) {
			  scalarHeader.add("Stress");
		  }

		  if(!baseLineTestRun.equals("-1")) {
			  scalarHeader.add("Baseline TR");  
		  }

		  if(!initialTestRun.equals("-1")) {
			  scalarHeader.add("Initial TR");  
		  }

		  if(!previousTestRun.equals("-1")) {
			  scalarHeader.add("Previous TR");
		  }
		  if(countForTrans != 0) {
			  scalarHeader.add("Success(%)");
		  }
		  scalarHeader.add("Current");
		  scalarHeader.add("Trend");
		  scalarHeader.add("Action");

		  testReport.setScalarHeaders(scalarHeader);


		  return testReport;
	  } 
	  catch(Exception ex)
	  {
		  logger.log(Level.SEVERE, "Error in parsing previous or baseline metrics stats" );
		  logger.log(Level.SEVERE, "---" + ex.getMessage());
		  return null;
	  }
  }

  
  
  private ArrayList<MetricData> parsePreviousAndBaseLineData(JSONObject jsonGraphData, int freq, String type) {
    try {
      logger.log(Level.INFO, "parsePreviousAndBaseLineData method in Integration  called for type = " + type);

      ArrayList<MetricData> listData = new ArrayList<MetricData>();

      Set keySet = jsonGraphData.keySet();

      if (keySet.size() < 1) {
        logger.log(Level.INFO, "Graph Metrics is not available in integration for " + type);
        return null;
      }

      Iterator itrTest = keySet.iterator();

      while (itrTest.hasNext()) {
        Object keyValue = itrTest.next();

        if (jsonGraphData.get(keyValue) == null) {
          return null;
        }

        JSONObject graphWithDataJson = (JSONObject) jsonGraphData.get(keyValue);

        Set keys = graphWithDataJson.keySet();
        Iterator itr = keys.iterator();

        while (itr.hasNext()) {
          String key = (String) itr.next();
          MetricData metricData = new MetricData();

          JSONObject graphJsonObj = (JSONObject) graphWithDataJson.get(key);
          String graphName = (String) graphJsonObj.get("graphMetricPath");

          metricData.setMetricPath(graphName.substring(graphName.indexOf("|") + 1));
          metricData.setFrequency(String.valueOf(freq));

          JSONArray jsonArray = (JSONArray) graphJsonObj.get("graphMetricValues");

          ArrayList<MetricValues> list = new ArrayList<MetricValues>();

          for (Object jsonArray1 : jsonArray) {
            MetricValues values = new MetricValues();
            JSONObject graphValues = (JSONObject) jsonArray1;
            String currVal = String.valueOf(graphValues.get("currentValue"));
            String maxVal = String.valueOf(graphValues.get("maxValue"));
            String minVal = String.valueOf(graphValues.get("minValue"));
            String avg = String.valueOf(graphValues.get("avgValue"));
            long timeStamp = (Long) graphValues.get("timeStamp");
            values.setValue((Double) graphValues.get("currentValue"));
            values.setMax((Double) graphValues.get("maxValue"));
            values.setMin(getMinForMetric((Double) graphValues.get("minValue")));
            values.setStartTimeInMillis(timeStamp);
            list.add(values);
          }

          metricData.setMetricValues(list);
          listData.add(metricData);
        }
      }
      return listData;
    } catch (Exception ex) {
      logger.log(Level.SEVERE, "NSNDIntegration  - Error in parsing previous or baseline metrics stats");
      logger.log(Level.SEVERE, "NSNDIntegration ---" + ex.getMessage());
      return null;
    }
  }

  private double getMinForMetric(double metricValue) {
    if (metricValue == Double.MAX_VALUE) {
      return 0.0;
    } else {
      return metricValue;
    }
  }

  private static enum JSONKeys {

    URLCONNECTION("URLConnectionString"), USERNAME("username"), PASSWORD("password"), PROJECT("project"), SUBPROJECT("subproject"), OPERATION_TYPE("Operation"),
    SCENARIO("scenario"), STATUS("Status"), TEST_RUN("TESTRUN"),
    TESTMODE("testmode"), GETPROJECT("PROJECTLIST"), GETSUBPROJECT("SUBPROJECTLIST"), GETSCENARIOS("SCENARIOLIST"), BASELINE_TR("baselineTR"), REPORT_STATUS("reportStatus");

    private final String value;

    JSONKeys(String value) {
      this.value = value;
    }

    public String getValue() {
      return value;
    }
  }

  private static enum OperationType {
    START_TEST, AUTHENTICATE_USER, GETDATA, GETPROJECT, GETSUBPROJECT, GETSCENARIOS
  };

  /*Method to make the json request data for netstorm post request.*/
  public JSONObject makeRequestObject(String type) {
    JSONObject jsonRequest = new JSONObject();
    if (type.equals("GET_DATA")) {
      jsonRequest.put(JSONKeys.USERNAME.getValue(), nsUsername);
      jsonRequest.put(JSONKeys.PASSWORD.getValue(), nsPassword.getPlainText());
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.GETDATA.toString());
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE);

    }
    return jsonRequest;
  }
}
