/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.security.KeyManagementException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
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


import java.util.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import hudson.util.Secret;

/**
 *
 * @author preety.yadav
 */
public class NetStormConnectionManager {
    
    private final String URLConnectionString;
    private transient final Logger logger = Logger.getLogger(NetStormConnectionManager.class.getName());
  private String servletName = "JenkinsServlet";
  private String username = "";
  private Secret password;
  private String project = "";
  private String subProject = "";
  private String scenario = "";
  private String testMode = "";
  private String duration;
  private String serverHost;
  private String vUsers;
  private String tName;
  private String rampUp;
  private String autoScript;
  private String htmlTablePath;
  private String baselineTR;
  private String result;
  private String err = "Authentication failure, please check whether username and password given correctly";

  private String pollURL;
  private String pollReportURL;
  private static int POLL_CONN_TIMEOUT = 60000;
  private static int POLL_REPEAT_TIME = 1 * 60000;
  private static int POLL_REPEAT_FOR_REPORT_TIME= 30000;
  private static int INITIAL_POLL_DELAY = 10000;
  private int testRun = -1;
  private String scenarioName = "NA";
  private PrintStream consoleLogger = null;
  private JSONObject resonseReportObj = null;
  private JSONObject jkRule = new JSONObject();
  
  private HashMap<String,String> slaValueMap =  new HashMap<String,String> ();


  public String getHtmlTablePath()
  {
    return htmlTablePath;
  }
  
  public void setHtmlTablePath(String htmlTablePath)
  {
    this.htmlTablePath = htmlTablePath;
  }
  
  public String getAutoScript()
  {
    return autoScript;
  }

  public void setAutoScript(String autoScript)
  {
    this.autoScript = autoScript;
  }
  
  public String gettName() {
    return tName;
  }

  public String getRampUp() {
    return rampUp;
  }

  
  public void settName(String tName) {
    this.tName = tName;
  }

  public void setRampUp(String rampUp) {
    this.rampUp = rampUp;
  }
  
  public String getBaselineTR() {
    return baselineTR;
  }

  public void setBaselineTR(String baselineTR) {
    this.baselineTR = baselineTR;
  }
  
  public void addSLAValue(String key, String value)
  {
    slaValueMap.put(key, value);
  }
  
  public void setDuration(String duration) {
    this.duration = duration;
  }

  public void setServerHost(String serverHost) {
    this.serverHost = serverHost;
  }

  public void setvUsers(String vUsers) {
    this.vUsers = vUsers;
  }

 
  public String getDuration() {
    return duration;
  }

  public String getServerHost() {
    return serverHost;
  }

  public String getvUsers() {
    return vUsers;
  } 
  
  public String getResult() {
     return result;
  }

  public void setResult(String result) {
     this.result = result;
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
  
  public JSONObject getJkRule() {
	 return jkRule;
  }

  public void setJkRule(JSONObject jkRule) {
	 this.jkRule = jkRule;
  }
  
  
  private static enum JSONKeys {

	 URLCONNECTION("URLConnectionString"),USERNAME("username"), PASSWORD("password"), PROJECT("project"), SUBPROJECT("subproject"), OPERATION_TYPE("Operation"),
    SCENARIO("scenario"), STATUS("Status"), TEST_RUN("TESTRUN"),
    TESTMODE("testmode"), GETPROJECT("PROJECTLIST") , GETSUBPROJECT("SUBPROJECTLIST"), GETSCENARIOS("SCENARIOLIST"), BASELINE_TR("baselineTR"), REPORT_STATUS("reportStatus"), ERROR_MSG("errMsg"), CHECK_RULE("checkRule");

    private final String value;

    JSONKeys(String value) {
      this.value = value;
    }

    public String getValue() {
      return value;
    }
  }

  private static enum OperationType
  {
    START_TEST, AUTHENTICATE_USER, GETDATA, GETPROJECT, GETSUBPROJECT, GETSCENARIOS
  };

  public NetStormConnectionManager(String URLConnectionString, String username, Secret password)
  {
    logger.log(Level.INFO, "NetstormConnectionManger constructor called with parameters with username:{0}", new Object[]{username});

    this.URLConnectionString = URLConnectionString;
    System.out.println("password NSConnection = "+password);
    this.username = username;
    this.password = password;
  }

  public NetStormConnectionManager(String URLConnectionString, String username, Secret password, String project, String subProject, String scenario, String testMode, String baselineTR)
  {
    logger.log(Level.INFO, "NetstormConnectionManger constructor called with parameters with username:{0}, project:{2}, subProject:{3}, scenario:{4}, baselineTR:{5}", new Object[]{username, project, subProject, scenario, baselineTR});

    this.URLConnectionString = URLConnectionString;
    this.username = username;
    this.project = project;
    this.subProject = subProject;
    this.scenario = scenario;
    this.testMode = testMode;
    this.baselineTR = baselineTR;
    System.out.println("password NSConnection2 = "+password);
    this.password = password;
    
  }
  
  /**
   * This method checks connection with netstorm
   *
   * @param urlString
   * @param servletPath
   * @param errMsg
   * @return true if connection successfully made false, otherwise
   */
  private boolean checkAndMakeConnection(String urlString, String servletPath, StringBuffer errMsg)
  {
    logger.log(Level.INFO, "checkAndMakeConnection method called. with arguments restUrl : ", new Object[]{urlString});
    try
      {
	 JSONObject reqObj = new JSONObject();
	 reqObj.put("username", this.username);
	 reqObj.put("password" ,this.password.getPlainText()); 
     reqObj.put("URLConnectionString", urlString);
         
	  URL url ;
	  String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
	  url = new URL(str+"/ProductUI/productSummary/jenkinsService/validateUser");
	     
	  logger.log(Level.INFO, "checkAndMakeConnection method called. with arguments url = "+  url);
	  HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	  conn.setRequestMethod("POST");
	      
	  conn.setRequestProperty("Accept", "application/json");
	  conn.setDoOutput(true);
	  String json =reqObj.toString();
	  System.out.println("json in NSConnection = "+json);
	  OutputStream os = conn.getOutputStream();
	  os.write(json.getBytes());
	  os.flush();

	   if (conn.getResponseCode() != 200) {
   	        throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
	   }
	   else 
	   {
	       BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
	       setResult(br.readLine());
	       logger.log(Level.INFO, "RESPONSE -> "+getResult());
	       return true;
	   }
	      
	} catch (MalformedURLException e) {
	      logger.log(Level.SEVERE, "Unknown exception in establishing connection. MalformedURLException -", e);
	      return false;
	} catch (IOException e) {
	      logger.log(Level.SEVERE, "Unknown exception in establishing connection. IOException -", e);
	      return false;
	} catch (Exception e) {
	      logger.log(Level.SEVERE, "Unknown exception in establishing connection.", e);
	      return (false);
	}
  
  }
    
  private void setDefaultSSLProperties(URLConnection urlConnection,StringBuffer errMsg)
  {
    try 
    {
      /*
       * For normal HTTP connection there is no need to set SSL properties.
       */
      if (urlConnection instanceof HttpsURLConnection)
      {
        /* We are not checking host name at time of SSL handshake. */
        HttpsURLConnection con = (HttpsURLConnection) urlConnection;
        con.setHostnameVerifier(new HostnameVerifier()
        {
          @Override
          public boolean verify(String arg0, SSLSession arg1) 
          {
            return true;
          }
        });
      }
    } 
    catch (Exception e) 
    {
    }
  }
  
  public JSONObject makeRequestObject(String type)
  {
    JSONObject jsonRequest = new JSONObject();
    
    if(type.equals("START_TEST")) 
    {
      jsonRequest.put(JSONKeys.USERNAME.getValue(), username);
      jsonRequest.put(JSONKeys.PASSWORD.getValue(), password.getPlainText()); 
      jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), URLConnectionString);
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.START_TEST.toString());
      jsonRequest.put(JSONKeys.PROJECT.getValue(), project);
      jsonRequest.put(JSONKeys.SUBPROJECT.getValue(), subProject);
      jsonRequest.put(JSONKeys.SCENARIO.getValue(), scenario);
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE);
      jsonRequest.put(JSONKeys.TESTMODE.getValue(), testMode);
      jsonRequest.put(JSONKeys.REPORT_STATUS.getValue(), ""); 
         
      
      if(getBaselineTR() != null && !getBaselineTR().trim().equals(""))
       {
         String baseline = getBaselineTR();
         if(baseline.startsWith("TR"))
	  baseline = baseline.substring(2, baseline.length());
	  
         jsonRequest.put("BASELINE_TR", baseline);
       } 
      else
       jsonRequest.put("BASELINE_TR", "-1");
       

      if(getDuration() != null && !getDuration().trim().equals(""))
      {
        jsonRequest.put("DURATION", getDuration());
      }
      
      if(getServerHost() != null && !getServerHost().trim().equals(""))
      {
        jsonRequest.put("SERVER_HOST", getServerHost());
      }
      
      if(getvUsers() != null && !getvUsers().trim().equals(""))
      {
        jsonRequest.put("VUSERS", getvUsers());
      }
      
      if(getRampUp() != null && !getRampUp().trim().equals(""))
      {
        jsonRequest.put("RAMP_UP", getRampUp());
      }
      
      if(gettName()!= null && !gettName().trim().equals(""))
      {
        jsonRequest.put("TNAME", gettName());
      }
      if(getAutoScript()!= null && !getAutoScript().trim().equals(""))
      {
        jsonRequest.put("AUTOSCRIPT", getAutoScript());
      }
      
      if(slaValueMap.size() > 0)
      {
        JSONArray  jsonArray = new JSONArray();
        Set<String> keyset = slaValueMap.keySet();
        
        for(String rule : keyset)
        {
          JSONObject jsonRule = new  JSONObject();
          jsonRule.put(rule, slaValueMap.get(rule));
          jsonArray.add(jsonRule);
        }
        
        jsonRequest.put("SLA_CHANGES", jsonArray);
      }
    }
    else if(type.equals("TEST_CONNECTION"))
    {
      jsonRequest.put(JSONKeys.USERNAME.getValue(), username);
      jsonRequest.put(JSONKeys.PASSWORD.getValue(), password.getPlainText());
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.AUTHENTICATE_USER.toString());
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE);
    }
    else if(type.equals("GET_DATA"))
    {
      jsonRequest.put(JSONKeys.USERNAME.getValue(), username);
      jsonRequest.put(JSONKeys.PASSWORD.getValue(), password.getPlainText());
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.GETDATA.toString());
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE); 
      jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), URLConnectionString);   
        
      //This is used get html report netstorm side.
      if(getHtmlTablePath() != null && !"".equals(getHtmlTablePath()))
        jsonRequest.put("REPORT_PATH", getHtmlTablePath());
      
    }
    else if(type.equals("GET_PROJECT"))
    {
      jsonRequest.put(JSONKeys.USERNAME.getValue(), username);
      jsonRequest.put(JSONKeys.PASSWORD.getValue(), password.getPlainText());
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.GETPROJECT.toString());
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE);
      jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), URLConnectionString); 
    }
    else if(type.equals("GET_SUBPROJECT"))
    {
      jsonRequest.put(JSONKeys.USERNAME.getValue(), username);
      jsonRequest.put(JSONKeys.PASSWORD.getValue(), password.getPlainText());
      jsonRequest.put(JSONKeys.PROJECT.getValue(), project);
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.GETSUBPROJECT.toString());
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE);
      jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), URLConnectionString); 
    }
    else if(type.equals("GET_SCENARIOS"))
    {
      jsonRequest.put(JSONKeys.USERNAME.getValue(), username);
      jsonRequest.put(JSONKeys.PASSWORD.getValue(), password.getPlainText());
      jsonRequest.put(JSONKeys.PROJECT.getValue(), project);
      jsonRequest.put(JSONKeys.SUBPROJECT.getValue(), subProject);
      jsonRequest.put(JSONKeys.TESTMODE.getValue(), testMode);
     jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), URLConnectionString);
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.GETSCENARIOS.toString());
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE);
    } 
    return jsonRequest;
  }
  

  /**
   * This Method makes connection to netstorm.
   *
   * @param errMsg
   * @return true , if Successfully connected and authenticated false ,
   * otherwise
   */
  public boolean testNSConnection(StringBuffer errMsg) 
  {
    logger.log(Level.INFO, "testNSConnection() called.");

    if(checkAndMakeConnection(URLConnectionString, servletName, errMsg))
    {
      
      JSONObject jsonResponse  =  (JSONObject) JSONSerializer.toJSON(getResult());
      
      if((jsonResponse == null))
      { 
        logger.log(Level.INFO, "Connection failure, please check whether Connection URI is specified correctly");
        errMsg.append("Connection failure, please check whether Connection URI is specified correctly");
        return false;
      }
      
      boolean status = (Boolean)jsonResponse.get(JSONKeys.STATUS.getValue());
     
      if(!jsonResponse.get(JSONKeys.ERROR_MSG.getValue()).equals(""))
        err = (String) jsonResponse.get(JSONKeys.ERROR_MSG.getValue()); 
        
      if (status)
      { 
        logger.log(Level.INFO, "Successfully Authenticated.");
        return true;
      }
      else
      { 
        logger.log(Level.INFO, "Authentication failure, please check whether username and password given correctly");
        errMsg.append(err);
      }
    }
    else
    { 
      logger.log(Level.INFO, "Connection failure, please check whether Connection URI is specified correctly");
      errMsg.append("Connection failure, please check whether Connection URI is specified correctly");
    }
      return false;
  }
   
   
  
  public ArrayList<String> getProjectList(StringBuffer errMsg)
  {
    logger.log(Level.INFO, "getProjectList method called.");
    
    try
    {
      logger.log(Level.INFO, "Making connection to Netstorm with following request uri- " + URLConnectionString);
      logger.log(Level.INFO, "Sending requets to get project list - " + URLConnectionString);
      JSONObject jsonResponse  = null;
//      if(checkAndMakeConnection(URLConnectionString, servletName, errMsg))
//      {   
    	JSONObject jsonRequest =    makeRequestObject("GET_PROJECT");
    	        
        try {
              URL url ;
              String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
    	      url = new URL(str+"/ProductUI/productSummary/jenkinsService/getProject");
    	     
    	      logger.log(Level.INFO, "getProjectList. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
    	      conn.setRequestMethod("POST");
    	      
    	      conn.setRequestProperty("Accept", "application/json");
    	      String json =jsonRequest.toString();
    	      conn.setDoOutput(true);
    	      OutputStream os = conn.getOutputStream();
    	      os.write(json.getBytes());
    	      os.flush();
    	      
    	      
    	      if (conn.getResponseCode() != 200) {
    		  throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
    	      }

    	      BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
    	      setResult(br.readLine());
    	      logger.log(Level.INFO, "RESPONSE for metric getProjectList  -> "+getResult());
    	      
    	      
    	       jsonResponse  =  (JSONObject) JSONSerializer.toJSON(this.result);
        	
          }
          catch (MalformedURLException e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection. MalformedURLException -", e);    	      
    	 } catch (IOException e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection. IOException -", e);    	    
    	 } catch (Exception e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection.", e);
    	 }
           
        if(jsonResponse != null)
        {
          if(jsonResponse.get(JSONKeys.STATUS.getValue()) != null && jsonResponse.get(JSONKeys.GETPROJECT.getValue()) !=null)
          {
            boolean status = (Boolean)jsonResponse.get(JSONKeys.STATUS.getValue());
            JSONArray projectJsonArray= (JSONArray)(jsonResponse.get(JSONKeys.GETPROJECT.getValue()));
           
            if(status == true)
            {
              ArrayList<String> projectList = new ArrayList<String>();
             
              for(int i = 0 ; i < projectJsonArray.size() ; i++)
              {
                 projectList.add((String)projectJsonArray.get(i));
              }
            
              return projectList;
            }
            else
            {
              logger.log(Level.INFO, "Not able to fetch project list from - " + URLConnectionString);
            }
          }
        }     
     
    }
    catch (Exception ex)
    {
      logger.log(Level.SEVERE, "Exception in getting project list ", ex);
    }
    
    return null;
  }

  public ArrayList<String> getSubProjectList(StringBuffer errMsg , String project)
  {
    logger.log(Level.INFO, "getSubProjectList method called.");
    
    try
    {
      logger.log(Level.INFO, "Making connection to Netstorm with following request uri- " + URLConnectionString);

      this.project =  project;
      JSONObject jsonResponse  = null;
//      if (checkAndMakeConnection(URLConnectionString, servletName, errMsg))
//      {
    	JSONObject jsonRequest =    makeRequestObject("GET_SUBPROJECT");
     
        try {
              URL url ;
              String str =  getUrlString(); //URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
    	      url = new URL(str+"/ProductUI/productSummary/jenkinsService/getSubProject");
    	     
    	      logger.log(Level.INFO, "getSubProjectList. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
    	      conn.setRequestMethod("POST");
    	      
    	      conn.setRequestProperty("Accept", "application/json");
    	      
    	      String json =jsonRequest.toString();
    	      conn.setDoOutput(true);
    	      OutputStream os = conn.getOutputStream();
    	      os.write(json.getBytes());
    	      os.flush();
    	      
    	      if (conn.getResponseCode() != 200) {
    		  throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
    	      }

    	      BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
    	      setResult(br.readLine());
    	      logger.log(Level.INFO, "RESPONSE for metric getSubProjectList  -> "+getResult());
    	      
    	      
    	       jsonResponse  =  (JSONObject) JSONSerializer.toJSON(this.result);
        	
          }
             catch (MalformedURLException e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection. MalformedURLException -", e);
    	      
    	    } catch (IOException e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection. IOException -", e);
    	    
    	    } catch (Exception e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection.", e);
    	    
    	 }
        
        if(jsonResponse != null)
        {
          if(jsonResponse.get(JSONKeys.STATUS.getValue()) != null && jsonResponse.get(JSONKeys.GETSUBPROJECT.getValue()) !=null)
          {
            boolean status = (Boolean)jsonResponse.get(JSONKeys.STATUS.getValue());
            JSONArray subProjectJSONArray= (JSONArray)(jsonResponse.get(JSONKeys.GETSUBPROJECT.getValue()));
            if(status == true)
            {
              ArrayList<String> subProjectList = new ArrayList<String>();
              for(int i = 0 ; i < subProjectJSONArray.size() ; i++)
              {
                 subProjectList.add((String)subProjectJSONArray.get(i));
              }
             
              return subProjectList;
            }
            else
            {
              logger.log(Level.SEVERE, "Not able to get sub project from - " + URLConnectionString);
            }
          }
       }
     
   }
   catch (Exception ex)
   {
     logger.log(Level.SEVERE, "Exception in getting getSubProjectList.", ex);
   }   
 
   return null;
 }  
  
  public ArrayList<String> getScenarioList(StringBuffer errMsg , String project, String subProject, String mode)
  {
    logger.log(Level.INFO, "getScenarioList method called.");
    try
    {
      logger.log(Level.INFO, "Making connection to Netstorm with following request uri- " + URLConnectionString);
      this.project = project;
      this.subProject = subProject;
      this.testMode = mode;
   
      JSONObject jsonResponse  = null;
//      if (checkAndMakeConnection(URLConnectionString, servletName, errMsg))
//      {
    	JSONObject jsonRequest =    makeRequestObject("GET_SCENARIOS");
        
        try {
              URL url ;
              String str =  getUrlString(); //URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
    	      url = new URL(str+"/ProductUI/productSummary/jenkinsService/getScenario");
    	     
    	      logger.log(Level.INFO, "getScenarioList. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
    	      conn.setRequestMethod("POST");
    	      
    	      conn.setRequestProperty("Accept", "application/json");
    	
    	      String json =jsonRequest.toString();
    	      conn.setDoOutput(true);
    	      OutputStream os = conn.getOutputStream();
    	      os.write(json.getBytes());
    	      os.flush();
    	            
    	      if (conn.getResponseCode() != 200) {
    		  throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
    	      }

    	      BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
    	      setResult(br.readLine());
    	      logger.log(Level.INFO, "RESPONSE for metric getScenarioList  -> "+getResult());
                
    	      
    	       jsonResponse  =  (JSONObject) JSONSerializer.toJSON(this.result);	
            }
             catch (MalformedURLException e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection. MalformedURLException -", e);
    	      
    	    } catch (IOException e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection. IOException -", e);
    	    
    	    } catch (Exception e) {
    	      logger.log(Level.SEVERE, "Unknown exception in establishing connection.", e);
    	 } 
    	  
       if(jsonResponse != null)
       {
         if(jsonResponse.get(JSONKeys.STATUS.getValue()) != null && jsonResponse.get(JSONKeys.GETSCENARIOS.getValue()) !=null)
         {
           
           boolean status = (Boolean)jsonResponse.get(JSONKeys.STATUS.getValue());
           JSONArray scenarioJSONArray= (JSONArray)(jsonResponse.get(JSONKeys.GETSCENARIOS.getValue()));
           
           if(status == true)
            {
              ArrayList<String> scenarioList = new ArrayList<String>();
              for(int i = 0 ; i < scenarioJSONArray.size() ; i++)
              {
                 scenarioList.add((String)scenarioJSONArray.get(i));
              }
             
              return scenarioList;
            }
            else
            {
              logger.log(Level.SEVERE, "Not able to get scenarios from - " + URLConnectionString);
            }
         }
        }
      
    }
    catch (Exception ex)
   {
     logger.log(Level.SEVERE, "Exception in getting getScenario.", ex);
   }   
  
   return null;
  }

  public HashMap startNetstormTest(StringBuffer errMsg , PrintStream consoleLogger)
  {
	  logger.log(Level.INFO, "startNetstormTest() called.");

	  this.consoleLogger = consoleLogger; 
	  HashMap resultMap = new HashMap(); 
	  resultMap.put("STATUS", false);

	  try 
	  {
		  logger.log(Level.INFO, "Making connection to Netstorm with following request uri- " + URLConnectionString);
		  consoleLogger.println("Making connection to Netstorm with following request uri- " + URLConnectionString);
		  JSONObject jsonResponse =null;
		  //      if (checkAndMakeConnection(URLConnectionString, servletName, errMsg))
		  //      { 
		  JSONObject jsonRequest =    makeRequestObject("START_TEST");
		  consoleLogger.println("Starting Test ... ");

		  try {
			  URL url;
			  String str =   getUrlString();//URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));

			  logger.log(Level.INFO, "this.jkRule- " + this.jkRule);    
			  url = new URL(str+"/ProductUI/productSummary/jenkinsService/startTest");

			  logger.log(Level.INFO, "startNetstormTest. method called. with arguments for metric  url"+  url);
			  HttpURLConnection conn = (HttpURLConnection) url.openConnection();
			  conn.setConnectTimeout(0);
			  conn.setReadTimeout(0);
			  conn.setRequestMethod("POST");
			  conn.setRequestProperty("Accept", "application/json");

			  String json =jsonRequest.toString();
			  conn.setDoOutput(true);
			  OutputStream os = conn.getOutputStream();
			  os.write(json.getBytes());
			  os.flush();

			  if (conn.getResponseCode() != 200) {
				  consoleLogger.println("Failed to Start Test with Error Code = " +  conn.getResponseCode());
				  logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
				  throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
			  }

			  consoleLogger.println("Test is Started Successfully. Now waiting for Test to End ...");

			  BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
			  setResult(br.readLine());

			  logger.log(Level.INFO, "RESPONSE for metric startNetstormTest  -> " + this.result.toString());
			  jsonResponse = (JSONObject) JSONSerializer.toJSON(this.result);

			  /*Getting scenario name from server.*/
			  if (jsonResponse.containsKey("scenarioName")) {
				  scenarioName = jsonResponse.get("scenarioName").toString();
			  }

			  /*Here checking the scenario Name of server.*/
			  if (scenarioName == null || scenarioName.equals("NA")) {
				  consoleLogger.println("Getting Empty Response from server. Something went wrong.");     
			  }

			  logger.log(Level.INFO, "Here starting the thread for checking the running scenario of server.");

			  /*Creating URL for polling.*/
			  pollURL = str + "/ProductUI/productSummary/jenkinsService/checkConnectionStatus";

			  /*Starting Thread and polling to server till test end.*/
			  connectNSAndPollTestRun();
			  consoleLogger.println("Getting Netstorm Report. It may take some time. Please wait...");

			  /*Setting TestRun here.*/
			  jsonResponse.put("TESTRUN", testRun + "" );
			  jsonResponse.put("REPORT_STATUS", "");

			  if(testRun == -1)
			  {

				  logger.log(Level.INFO, "Test is Failed .");
				  consoleLogger.println("Test is either not started or failed due to some reason,");
				  return resultMap;            
			  }

			  /** if check rule file is imported then call this method. */
			  if(this.jkRule != null) {
				  createCheckRuleFile(str);
			  }
			  logger.log(Level.INFO, "Test is Ended. Now checking the server response.");
			  parseTestResponseData(jsonResponse, resultMap, consoleLogger);

		  }	
		  catch (MalformedURLException e) {
			  logger.log(Level.SEVERE, "Unknown exception in establishing connection. MalformedURLException -", e);
		  } catch (IOException e) {
			  logger.log(Level.SEVERE, "Unknown exception in establishing connection. IOException -", e);
		  } catch (Exception e) {
			  logger.log(Level.SEVERE, "Unknown exception in establishing connection.", e);
		  }

	  }
	  catch (Exception ex) 
	  {
		  logger.log(Level.SEVERE, "Exception in closing connection.", ex);
		  return resultMap;
	  }

	  return resultMap;
  }

   public void createCheckRuleFile(String restUrl) {
	 try {
	   logger.log(Level.INFO, "inside createCheckRuleFile ");
	   JSONObject jsonRequest = new JSONObject();
	   jsonRequest.put(JSONKeys.PROJECT.getValue(), project);
	   jsonRequest.put(JSONKeys.SUBPROJECT.getValue(), subProject);
	   jsonRequest.put(JSONKeys.SCENARIO.getValue(), scenario);
	   jsonRequest.put(JSONKeys.TEST_RUN.getValue(), testRun + "");
	   jsonRequest.put(JSONKeys.CHECK_RULE.getValue(), this.jkRule);
	   URL url;
	   url = new URL(restUrl+"/ProductUI/productSummary/jenkinsService/createCheckProfile");
	
	   HttpURLConnection conn = (HttpURLConnection) url.openConnection();
       conn.setConnectTimeout(0);
       conn.setReadTimeout(0);
       conn.setRequestMethod("POST");
       conn.setRequestProperty("Accept", "application/json");

       String json =jsonRequest.toString();
       conn.setDoOutput(true);
       OutputStream os = conn.getOutputStream();
       os.write(json.getBytes());
       os.flush();

   if (conn.getResponseCode() != 200) {
    consoleLogger.println("Failed to write check rule with Error Code = " +  conn.getResponseCode());
    logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
    throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
   }

	 } catch(Exception e) {
		 logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
	 }
   }
 /**
   * Method is used for parsing the response of server from test start.
   * @param jsonResponse
   * @param resultMap
   * @param consoleLogger
   */
  public void parseTestResponseData(JSONObject jsonResponse, HashMap resultMap, PrintStream consoleLogger) {
    try {
      
      consoleLogger.println("Processing Server Response ....");
      
      if(jsonResponse != null) {
	boolean status = false;
	if(jsonResponse.get(JSONKeys.STATUS.getValue()) != null)
	{
	  status = (Boolean)jsonResponse.get(JSONKeys.STATUS.getValue()); 
	  if(!status)
	  {
	    consoleLogger.println("Test is aborted."); 
	  }
	}

	//Changes for showing shell output on jenkins console.
	if(jsonResponse.get(JSONKeys.REPORT_STATUS.getValue()) != null) {

	  String repotStatus = (String)(jsonResponse.get(JSONKeys.REPORT_STATUS.getValue()));
	  consoleLogger.println(repotStatus); 
	}

	if(jsonResponse.get(JSONKeys.TEST_RUN.getValue()) != null) {
	  String testRun= (String)(jsonResponse.get(JSONKeys.TEST_RUN.getValue()));
	  resultMap.put("STATUS", status);
	  resultMap.put("TESTRUN",testRun);

	  if(jsonResponse.containsKey("ENV_NAME"))
	  { 
	    String envNames = "";
	    JSONArray envArr = (JSONArray)jsonResponse.get("ENV_NAME");

	    for(int i = 0 ; i < envArr.size() ; i++)
	    { 
	      if( i == 0)
		envNames = (String)envArr.get(i);
	      else
		envNames = envNames + "," + (String)envArr.get(i);
	    }

	    resultMap.put("ENV_NAME", envNames);
	  }       
	  consoleLogger.println("Test is executed successfully.");
	}
      }
    } catch (Exception ex) {
      logger.log(Level.SEVERE, "Exception in parsing netstorm test start output.", ex);
    }
  }

  /**
   * Method is used for checking connection with netstorm and polling netstorm for testrun running status based on scenario name.
   * @param scenarioName
   * @param consoleLogger
   */
  private void connectNSAndPollTestRun() {
    try {

      consoleLogger.println("Test Started. Now tracking TestRun based on running scenario. URL = " + pollURL);
      
      /* Creating the thread. */
      Runnable pollTestRunState = new Runnable()
      {
        public void run()
        {
          try {

            /*Keeping flag based on TestRun status on server.*/
            boolean isTestRunning = true;
            
            /*Initial Sleep Before Polling.*/
            try {
              
              /*Delay to poll due to test is taking time to start.*/
              Thread.sleep(INITIAL_POLL_DELAY);     
              
            } catch (Exception ex) {
              logger.log(Level.SEVERE, "Error in initial sleep before polling.", ex);
            }

            logger.log(Level.INFO, "Starting Polling to server.");
            
            /*Running Thread till test stopped.*/
            while (isTestRunning) {
              try {
        	
        	/*Creating Polling URL.*/
        	String pollURLWithArgs = pollURL + "?proSubProject=" + scenarioName + "&testRun=" + testRun;    	
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
        	      	  
        	  /*Getting TestRun, if not available.*/
        	  if (testRun <= 0) {
        	    testRun = pollResponse.getInt("testRun");	    
        	  }
        	  
        	  if(pollResponse.getBoolean("status")) {
        	    /*Terminating Loop when test is stopped.*/
        	    isTestRunning = false;
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
                logger.log(Level.SEVERE, "Error in polling running testRun with interval. Retrying after 5 sec.", e);
              }

              /*Repeating till Test Ended.*/
              try {
        	Thread.sleep(POLL_REPEAT_TIME);
                consoleLogger.println("Test in progress. Going to check on server. Time = " + new Date());
              } catch (Exception ex) {       	
        	logger.log(Level.SEVERE, "Error in polling connection in loop", ex);
              } 
            }

          } catch (Exception e) {
            logger.log(Level.SEVERE, "Error in polling running testRun with interval.", e);
          }
        }   
      };

      // Creating and Starting thread for Getting Graph Data.
      Thread pollTestRunThread = new Thread(pollTestRunState, "pollTestRunThread");

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
      
      consoleLogger.println("TestRun is stopped. Now checking the server state.");
      
    } catch (Exception e) {
      logger.log(Level.SEVERE, "Error in polling running testRun.", e);
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
                      JSONParser parser = new JSONParser();
                      org.json.simple.JSONObject objJson = (org.json.simple.JSONObject) parser.parse(pollResString);
                       String strData = objJson.get("data").toString();
                      
                       org.json.simple.JSONObject objJson2 =(org.json.simple.JSONObject)parser.parse(strData);
                       String strData2 = objJson2.toJSONString();
                       logger.log(Level.INFO, "Data -- = " + strData);
                       resonseReportObj = (JSONObject) JSONSerializer.toJSON(strData2);
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



  public MetricDataContainer fetchMetricData(NetStormConnectionManager connection,  String metrics[], String durationInMinutes, int groupIds[], int graphIds[], int testRun, String testMode)
  {
    logger.log(Level.INFO, "fetchMetricData() called.");
  
    JSONObject jsonRequest = makeRequestObject("GET_DATA");
    
    logger.log(Level.INFO, "json request----->",jsonRequest);
    jsonRequest.put("TESTRUN", String.valueOf(testRun));
    jsonRequest.put(JSONKeys.TESTMODE.getValue(), testMode);
    jsonRequest.put(JSONKeys.PROJECT.getValue(), connection.getProject());
    jsonRequest.put(JSONKeys.SUBPROJECT.getValue(), connection.getSubProject());
    jsonRequest.put(JSONKeys.SCENARIO.getValue(), connection.getScenario());
   
    this.testRun = testRun; 
    JSONArray jSONArray = new JSONArray();
     
    for(int i = 0 ; i < metrics.length ; i++)
    {
      jSONArray.add(groupIds[i] + "." + graphIds[i]);
    }
   
    jsonRequest.put("Metric", jSONArray);
    logger.log(Level.INFO, "Metric json array --> "+jSONArray);
    
    logger.log(Level.INFO, "Test Run --> "+String.valueOf(testRun));
   
    StringBuffer errMsg = new StringBuffer();
    JSONObject resonseObj = null;
    
//    if(checkAndMakeConnection(URLConnectionString, servletName, errMsg))
//    {
      try {
    	    URL url ;
    	    String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
	    url = new URL(str+"/ProductUI/productSummary/jenkinsService/jsonData");
	     
	    logger.log(Level.INFO, "fetchMetricData.  method called. with arguments for metric  url"+  url);
	    HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setConnectTimeout(0);
            conn.setReadTimeout(0);
	    conn.setRequestMethod("POST");
        
	    conn.setRequestProperty("Accept", "application/json");
        
	    String json =jsonRequest.toString();
	    conn.setDoOutput(true);
	    OutputStream os = conn.getOutputStream();
	    os.write(json.getBytes());
	    os.flush();

	    if (conn.getResponseCode() != 200) {
	      throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
	    }

            BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
            resonseReportObj =  (JSONObject) JSONSerializer.toJSON(br.readLine());
	    logger.log(Level.INFO, "Response for getting Json report   -> "+resonseReportObj);

            if(resonseReportObj.containsKey("status"))
            {
               if(resonseReportObj.getBoolean("status") == false)
               {
                 logger.log(Level.SEVERE, "Not able to get response form server due to some reason");
                 consoleLogger.println("Error in report generation.");
                 return null;
               }
            }
        
            pollReportURL = str+"/ProductUI/productSummary/jenkinsService/checkNetstormReportStatus";
        
            logger.log(Level.INFO, "url for polling report - pollReportURL = " + pollReportURL);
            
            connectNSAndPollJsonReport();
        
            if(resonseReportObj == null)
            {
               logger.log(Level.SEVERE, "Not able to get response form server due to: " + errMsg);
               return null;
            }
         }
         catch (MalformedURLException e) {
	   logger.log(Level.SEVERE, "Unknown exception in establishing connection. MalformedURLException -", e);
         } catch (IOException e) {
	   logger.log(Level.SEVERE, "Unknown exception in establishing connection. IOException -", e);
        } catch (Exception e) {
	     logger.log(Level.SEVERE, "Unknown exception in establishing connection.", e);
        }
//     }
//    else
//    {
//      logger.log(Level.INFO, "Connection failure, please check whether Connection URI is specified correctly");
//      errMsg.append("Connection failure, please check whether Connection URI is specified correctly");
//      return null;
//    }

    return parseJSONData(resonseReportObj, testMode);
  }  


  private MetricDataContainer parseJSONData(JSONObject resonseObj, String testMode)
  {
    logger.log(Level.INFO, "parseJSONData() called.");
    
    MetricDataContainer metricDataContainer = new MetricDataContainer();
    logger.log(Level.INFO,"Metric Data:" + metricDataContainer );
    logger.log(Level.INFO,"Recived response from : " + resonseObj );
    System.out.println("Recived response from : " + resonseObj);
    
    try
    {
      ArrayList<MetricData> dataList = new ArrayList<MetricData>();
      
      JSONObject jsonGraphs = (JSONObject)resonseObj.get("graphs");
      int freq = ((Integer)resonseObj.get("frequency"))/1000; 
      metricDataContainer.setFrequency(freq);
      
      if(resonseObj.containsKey("customHTMLReport"))
        metricDataContainer.setCustomHTMLReport((String)resonseObj.get("customHTMLReport"));
      
      TestReport testReport = new TestReport();
      if("T".equals(testMode))
      {
        
        testReport = new TestReport();
        testReport.setUserName(username);
     
        JSONObject jsonTestReportWholeObj = resonseObj.getJSONObject("testReport");
        JSONObject jsonTestReport = jsonTestReportWholeObj.getJSONObject("members");
        
        String overAllStatus =  jsonTestReport.getString("Overall Status");
        String date = jsonTestReport.getString("Date");
        String overAllFailCriteria = jsonTestReport.getString("Overall Fail Criteria (greater than red) %");
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
    
        try
        {
          currentDateTime = jsonTestReport.getString("Current Date Time");
          previousDescription = jsonTestReport.getString("Previous Description"); 
          baselineDescription = jsonTestReport.getString("Baseline Description");
          currentDescription =  jsonTestReport.getString("Current Description");
          initialDescription = jsonTestReport.getString("Initial Description");
        }
        catch(Exception ex)
        {
          logger.log(Level.SEVERE, "Error in parsing Test Report Data:" + ex);
          logger.log(Level.SEVERE, "---" + ex.getMessage());
        }
       if(jsonTestReport.get("Metrics Under Test") != null) {
        JSONArray metricsUnderTest = (JSONArray)jsonTestReport.get("Metrics Under Test");
        ArrayList<TestMetrics> testMetricsList = new ArrayList<TestMetrics>(metricsUnderTest.size());
        
        String str = "";
        int index = 0; 
        
        for(Object jsonData : metricsUnderTest)
        {  
          JSONObject jsonObject = (JSONObject)jsonData;
          
          String prevTestValue = jsonObject.getString("Prev Test Value ");
          String baseLineValue = jsonObject.getString("Baseline Value ");
          String initialValue = jsonObject.getString("Initial Value ");
          String edLink = jsonObject.getString("link");
          String currValue = jsonObject.getString("Value");
          String metric = jsonObject.getString("Metric");
          String metricRule = jsonObject.getString("MetricRule");
          String operator = jsonObject.getString("Operator");
          String sla = jsonObject.getString("SLA");
          if(sla.indexOf(">") != -1 || sla.indexOf(">") > 0)
	    sla = sla.substring(sla.lastIndexOf(">")+1, sla.length());

          String transactiontStatus = jsonObject.getString("Transaction Status");
          String transactionBgcolor = jsonObject.getString("Transaction BGcolor");
          String transactionTooltip = jsonObject.getString("Transaction Tooltip"); 
          String trendLink = jsonObject.getString("trendLink");
          String metricLink = jsonObject.getString("Metric_DashboardLink");
          logger.log(Level.INFO," metric link ", metricLink);
          TestMetrics testMetric = new TestMetrics();
          
          testMetric.setBaseLineValue(baseLineValue);
          testMetric.setCurrValue(currValue);
          if(edLink != null)
            testMetric.setEdLink(edLink);
          else
            testMetric.setEdLink("NA");
          testMetric.setOperator(operator);
          testMetric.setPrevTestRunValue(prevTestValue);
          testMetric.setInitialValue(initialValue);
          testMetric.setSLA(sla);
          if(trendLink != null)
           testMetric.setLinkForTrend(trendLink);
          else
            testMetric.setLinkForTrend("NA");
            
          String headerName = "";
          String displayName = metric;
          if (index == 0)
          {
            str = displayName;
            if(displayName.contains("- All"))
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
                 if (displayName.contains("- All"))
                 {
                   displayName = displayName.substring(displayName.indexOf("-")+6,displayName.length()-1);
                 }
                 else
                   displayName = displayName.substring(displayName.indexOf("-")+1,displayName.length());
                }
               else
               {
                 str = displayName;
                 if (displayName.contains("- All"))
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
               
               if(displayName.contains("- All"))
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
          testMetric.setMetricLink(metricLink);
          testMetric.setMetricRuleName(metricRule);
          testMetric.setTransactiontStatus(transactiontStatus);
          testMetric.setStatusColour(transactionBgcolor);
          testMetric.setTransactionTooltip(transactionTooltip);
          testMetricsList.add(testMetric);
          testReport.setOperator(operator);
          testReport.setTestMetrics(testMetricsList);
        }
       } else {
    	   /* method calls for transaction stats, vector groups and scalar groups table */
    	   if(jsonTestReport.get("Transaction Stats") != null) {
    		JSONObject transStats = (JSONObject)jsonTestReport.get("Transaction Stats");
    	    testReport = metricDataForTrans(transStats, testReport, jsonTestReport);
    	   }
    	   if(jsonTestReport.get("Vector Groups") != null) {
    		JSONObject vectorGroups = (JSONObject)jsonTestReport.get("Vector Groups");
    		testReport = metricDataForVectorGroups(vectorGroups, testReport, jsonTestReport);
    	   }
    	   if(jsonTestReport.get("Scalar Groups") != null) {
    		JSONObject scalarGroups = (JSONObject)jsonTestReport.get("Scalar Groups");
            testReport = metricDataForScalar(scalarGroups, testReport, jsonTestReport);
    	   }
           
       }
        
        int transObj = 1;
        
        //Check is used for if Base transaction exist in json report.
        if(jsonTestReport.has("BASETOT"))
           transObj = 2;        
 
        for(int i = 0 ; i < transObj; i++)
        {
          JSONObject transactionJson = null;
          
          if(i == 1)
          {
            transactionJson = jsonTestReport.getJSONObject("BASETOT");
          }
          else 
          {  
            if(jsonTestReport.has("TOT"))
              transactionJson = jsonTestReport.getJSONObject("TOT");
            else
              transactionJson = jsonTestReport.getJSONObject("CTOT"); 
          }

          logger.log(Level.INFO, "transactionJson ="+transactionJson);
   
          String complete = "NA";
          if(transactionJson.getString("complete") != null)
            complete = transactionJson.getString("complete");
        
          String totalTimeOut = "NA";
          if(transactionJson.getString("Time Out") != null)
            totalTimeOut = transactionJson.getString("Time Out");
        
          String t4xx = "NA";
          if(transactionJson.getString("4xx") != null)
            t4xx = transactionJson.getString("4xx");
        
          String t5xx = "NA";
          if(transactionJson.getString("5xx") != null)
            t5xx = transactionJson.getString("5xx");
        
          String conFail = "NA";
          if(transactionJson.getString("ConFail") != null)
            conFail = transactionJson.getString("ConFail");
        
          String cvFail = "NA";
          if(transactionJson.getString("C.V Fail") != null)
            cvFail = transactionJson.getString("C.V Fail");
 
          String success = "NA";
          if(transactionJson.getString("success") != null)
            success = transactionJson.getString("success");
        
          String warVersionTrans = "NA";
          if(transactionJson.has("warVersion"))
            warVersionTrans = transactionJson.getString("warVersion");
          
          String releaseVersionTrans = "NA";
          if(transactionJson.has("releaseVersion"))
            releaseVersionTrans = transactionJson.getString("releaseVersion");

          //Create Transaction Stats Object to save Base Test and Current Test Run transaction details
          TransactionStats transactionStats = new TransactionStats();
        
          if(i == 1)
          {
            transactionStats.setTransTestRun("BASETOT");
          }
          else
          {
            if(jsonTestReport.has("TOT"))
              transactionStats.setTransTestRun("TOT");
            else
              transactionStats.setTransTestRun("CTOT");
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
 
          testReport.getTransStatsList().add(transactionStats);
        }
        
        testReport.setBaseLineTestRun(baseLineTestRun);
        testReport.setInitialTestRun(initialTestRun);
        testReport.setBaselineDateTime(baseLineDateTime);
        testReport.setPreviousDateTime(previousDateTime);
        testReport.setInitialDateTime(initialDateTime);
        testReport.setOverAllFailCriteria(overAllFailCriteria);
        testReport.setDate(date);

        testReport.setDashboardURL(dashboardURL);
        testReport.setReportLink(reportLink);
//        testReport.setTestMetrics(testMetricsList);
        testReport.setOverAllStatus(overAllStatus);
        testReport.setServerName(serverName);
        testReport.setPreviousTestRun(previousTestRun);
        testReport.setTestRun(testRun);
        testReport.setNormalThreshold(normalThreshold);
        testReport.setCriticalThreshold(criticalThreshold);
        testReport.setCurrentDateTime(currentDateTime);
        testReport.setPreviousDescription(previousDescription);
        testReport.setBaselineDescription(baselineDescription);
        testReport.setIpPortLabel(productName);
        testReport.setInitialDescription(initialDescription);
        testReport.setCurrentDescription(currentDescription);
        metricDataContainer.setTestReport(testReport);
      }
      if(jsonGraphs != null)
      {
      Set keySet  = jsonGraphs.keySet();
      Iterator itr = keySet.iterator();
    
      while(itr.hasNext())
      {
        String key = (String)itr.next();
        MetricData metricData = new MetricData();
      
        JSONObject graphJsonObj = (JSONObject)jsonGraphs.get(key);
        String graphName = (String)graphJsonObj.get("graphMetricPath");
    
        metricData.setMetricPath(graphName.substring(graphName.indexOf("|") + 1));
        metricData.setFrequency(String.valueOf(freq));
    
        JSONArray jsonArray = (JSONArray)graphJsonObj.get("graphMetricValues");
        
        ArrayList<MetricValues> list = new ArrayList<MetricValues>();
     
        for (Object jsonArray1 : jsonArray)
        {
          MetricValues values =  new MetricValues();
          JSONObject graphValues = (JSONObject) jsonArray1;
          String currVal = String.valueOf(graphValues.get("currentValue"));
          String maxVal  = String.valueOf(graphValues.get("maxValue"));
          String minVal = String.valueOf(graphValues.get("minValue"));
          String avg  = String.valueOf(graphValues.get("avgValue"));
          long timeStamp  = (Long)graphValues.get("timeStamp");
          values.setValue((Double)graphValues.get("currentValue"));
          values.setMax((Double)graphValues.get("maxValue"));
          values.setMin(getMinForMetric((Double)graphValues.get("minValue")));
          values.setStartTimeInMillis(timeStamp);
          list.add(values);
          
        }   
        metricData.setMetricValues(list);
        dataList.add(metricData); 
        metricDataContainer.setMetricDataList(dataList);
      }
      }
      
      //Now checking in response for baseline and previous test data
      if(testMode.equals("T"))
      {
        if(resonseObj.get("previousTestDataMap") != null)
        {
          JSONObject jsonGraphObj = (JSONObject)resonseObj.get("previousTestDataMap"); 
        
          ArrayList<MetricData> prevMetricDataList =  parsePreviousAndBaseLineData(jsonGraphObj , freq , "Previous Test Run");
        
          if((prevMetricDataList != null))
          {
            logger.log(Level.INFO, "Setting previous test data in metric container = "  + prevMetricDataList);
            metricDataContainer.setMetricPreviousDataList(prevMetricDataList);
          }
        }
      
        if(resonseObj.get("baseLineTestDataMap") != null)
        {
          JSONObject jsonGraphObj = (JSONObject)resonseObj.get("baseLineTestDataMap"); 
          ArrayList<MetricData> baseLineMetricDataList =  parsePreviousAndBaseLineData(jsonGraphObj, freq , "Base Line Test Run");
       
          if((baseLineMetricDataList != null))
          {
            logger.log(Level.INFO, "Setting baseline test data in metric container = " + baseLineMetricDataList);
            metricDataContainer.setMetricBaseLineDataList(baseLineMetricDataList);
          }
        }
      }
    }
    catch(Exception e)
    {
      logger.log(Level.SEVERE, "Error in parsing metrics stats" );
      logger.log(Level.SEVERE, "Metric Data:" + e);
      logger.log(Level.SEVERE, "---" + e.getMessage());
      return null;
    }
    
    logger.log(Level.INFO,"Metric Data:" + metricDataContainer );

          
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
						  int countForMetrices = counts+1;
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
					  int countForMetrices = count+1;
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

  private ArrayList<MetricData> parsePreviousAndBaseLineData(JSONObject jsonGraphData, int freq , String type)
  {
    try
    {
      logger.log(Level.INFO, "method called for type = " + type);
      
      ArrayList<MetricData> listData = new ArrayList<MetricData>();
      
      Set keySet  = jsonGraphData.keySet();
      
      if(keySet.size() < 1)
      {
        logger.log(Level.INFO, "Graph Metrics is not available for " + type);
        return null;
      }
      
      Iterator itrTest = keySet.iterator();

      while(itrTest.hasNext())
      {
        Object keyValue  = itrTest.next();   
        
        if(jsonGraphData.get(keyValue) == null)
          return null;
        
        JSONObject graphWithDataJson = (JSONObject)jsonGraphData.get(keyValue);
           
        Set keys  = graphWithDataJson.keySet();
        Iterator itr = keys.iterator();
           
        while(itr.hasNext())
        {
          String key = (String)itr.next();
          MetricData metricData = new MetricData();
      
          JSONObject graphJsonObj = (JSONObject)graphWithDataJson.get(key);
          String graphName = (String)graphJsonObj.get("graphMetricPath");
        
          metricData.setMetricPath(graphName.substring(graphName.indexOf("|") + 1));
          metricData.setFrequency(String.valueOf(freq));
    
          JSONArray jsonArray = (JSONArray)graphJsonObj.get("graphMetricValues");
      
          ArrayList<MetricValues> list = new ArrayList<MetricValues>();
      
          for (Object jsonArray1 : jsonArray)
          {
            MetricValues values =  new MetricValues();
            JSONObject graphValues = (JSONObject) jsonArray1;
            String currVal = String.valueOf(graphValues.get("currentValue"));
            String maxVal  = String.valueOf(graphValues.get("maxValue"));
            String minVal = String.valueOf(graphValues.get("minValue"));
            String avg  = String.valueOf(graphValues.get("avgValue"));
            long timeStamp  = (Long)graphValues.get("timeStamp");
            values.setValue((Double)graphValues.get("currentValue"));
            values.setMax((Double)graphValues.get("maxValue"));
            values.setMin(getMinForMetric((Double)graphValues.get("minValue")));
            values.setStartTimeInMillis(timeStamp);
            list.add(values);
           }     
           
           metricData.setMetricValues(list);
           listData.add(metricData);
        }
      }
      return listData;
    }
    catch(Exception ex)
    {
      logger.log(Level.SEVERE, "Error in parsing previous or baseline metrics stats" );
      logger.log(Level.SEVERE, "---" + ex.getMessage());
      return null;
    }
  }
  
  private double getMinForMetric(double metricValue) 
  {
    if(metricValue == Double.MAX_VALUE)
      return 0.0;
    else
      return metricValue;
  }


  public String getUrlString()
  {
    String urlAddrs = "";
    try{
    String str[] = URLConnectionString.split(":");
    if(str.length > 2)
      {
        urlAddrs = str[0]+":"+str[1];
        if(str[2].contains("/"))
        {
          String value[] = str[2].split("/");
          urlAddrs = urlAddrs +":" + value[0];

        }
        else
          urlAddrs = urlAddrs +":" +str[2];
      }
    else
     {
        urlAddrs = URLConnectionString;
     }

     }
      catch(Exception ex){
       logger.log(Level.SEVERE, "Error in getting url string " );
       return URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
      }

     return urlAddrs;
  }

  
  public static void main(String args[]) 
  {
    String[] METRIC_PATHS =  new String[]{"Transactions Started/Second", "Transactions Completed/Second", "Transactions Successful/Second", "Average Transaction Response Time (Secs)", "Transactions Completed","Transactions Success" };
    int graphId [] = new int[]{7,8,9,3,5,6};
    int groupId [] = new int[]{6,6,6,6,6,6};
   // NetStormConnectionManager ns = new NetStormConnectionManager("http://localhost:8080", "netstorm", "netsrm");
  
   // ns.testNSConnection(new StringBuffer());
   // ns.fetchMetricData(METRIC_PATHS, 0, groupId, graphId,3334, "N");
  }
    
}
