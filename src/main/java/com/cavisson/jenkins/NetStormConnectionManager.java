/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package com.cavisson.jenkins;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import java.net.URLEncoder;
import java.security.KeyManagementException;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.GZIPInputStream;

import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSession;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import net.sf.json.JSONArray;
import net.sf.json.JSONObject;
import net.sf.json.JSONSerializer;

import java.text.DecimalFormat;
import java.text.NumberFormat;

import org.json.simple.parser.JSONParser;


import java.util.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import hudson.FilePath;
import hudson.model.Result;
import hudson.model.Run;
import hudson.util.Secret;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import com.cavisson.jenkins.PageDetail;

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
  private String rampUpDuration;
  private String autoScript;
  private String htmlTablePath;
  private String baselineType;
  private String gitPull = "false";
  private long pollInterval;
  private String result;
  private String emailIdTo = "";
  private String emailIdCc = "";
  private String emailIdBcc = "";
  private String dataDir = "";
  private String err = "Authentication failure, please check whether username and password given correctly";

  private String pollURL;
  private String pollReportURL;
  private static int POLL_CONN_TIMEOUT = 60000;
  private static int POLL_REPEAT_TIME = 1 * 60000;
  private static int POLL_REPEAT_FOR_REPORT_TIME= 30000;
  private static int INITIAL_POLL_DELAY = 10000;
  private int testRun = -1;
  private String testCycleNum = "";
  private String scenarioName = "NA";
  private PrintStream consoleLogger = null;
  private JSONObject resonseReportObj = null;
  private JSONObject jkRule = null;
  private boolean durationPhase = false;
  private String profile = "";
  private String hiddenBox = "";
  private int timeout = -1;
  private boolean generateReport = true;
  private boolean doNotWaitforTestCompletion = false;
  List<String> testsuitelist = null;
  HashMap<String, ParameterDTO> testsuiteParameterDTO = null;
  private String job_id = "";
  private String errMsg = "";
  
  
  private HashMap<String,String> slaValueMap =  new HashMap<String,String> ();

  static
  { 
    disableSslVerification();
  }
  
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
  
  public String getBaselineType() {
    return baselineType;
  }

  public void setBaselineType(String baselineType) {
    this.baselineType = baselineType;
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
  
  public long getPollInterval() {
      return pollInterval;
  }
  
  public void setPollInterval(long pollInterval) {
     this.pollInterval = pollInterval;
  }
 
  public String getRampUpDuration() {
	return rampUpDuration;
  }

  public void setRampUpDuration(String rampUpDuration) {
	this.rampUpDuration = rampUpDuration;
  }

  public String getGitPull() {
	return gitPull;
  }

  public void setGitPull(String gitPull) {
	this.gitPull = gitPull;
  }
  
  public String getProfile() {
	return profile;
  }

  public void setProfile(String profile) {
	this.profile = profile;
  }
  
  public int getTimeout() {
	return timeout;
  }

  public void setTimeout(int timeout) {
	this.timeout = timeout;
  }

  public boolean isGenerateReport() {
	  return generateReport;
  }

  public void setGenerateReport(boolean generateReport) {
	  this.generateReport = generateReport;
  }
  
  public String getEmailIdTo() {
	  return emailIdTo;
  }

  public void setEmailIdTo(String emailIdTo) {
	  this.emailIdTo = emailIdTo;
  }

  public String getEmailIdCc() {
	  return emailIdCc;
  }

  public void setEmailIdCc(String emailIdCc) {
	  this.emailIdCc = emailIdCc;
  }

  public String getEmailIdBcc() {
	  return emailIdBcc;
  }

  public void setEmailIdBcc(String emailIdBcc) {
	  this.emailIdBcc = emailIdBcc;
  }

  public String getDataDir() {
	  return dataDir;
  }

  public void setDataDir(String dataDir) {
	  this.dataDir = dataDir;
  }

  public boolean isDoNotWaitforTestCompletion() {
	  return doNotWaitforTestCompletion;
  }

  public void setDoNotWaitforTestCompletion(boolean doNotWaitforTestCompletion) {
	  this.doNotWaitforTestCompletion = doNotWaitforTestCompletion;
  }

  public List<String> getTestsuitelist() {
	  return testsuitelist;
  }

  public void setTestsuitelist(List<String> testsuitelist) {
	  this.testsuitelist = testsuitelist;
  }

  public HashMap<String, ParameterDTO> getTestsuiteParameterDTO() {
	  return testsuiteParameterDTO;
  }

  public void setTestsuiteParameterDTO(HashMap<String, ParameterDTO> testsuiteParameterDTO) {
	  this.testsuiteParameterDTO = testsuiteParameterDTO;
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
  
  private static enum JSONKeys {

	 URLCONNECTION("URLConnectionString"),USERNAME("username"), PASSWORD("password"), PROJECT("project"), SUBPROJECT("subproject"), OPERATION_TYPE("Operation"),
    SCENARIO("scenario"), STATUS("Status"), TEST_RUN("TESTRUN"),
    TESTMODE("testmode"), GETPROJECT("PROJECTLIST") , GETSUBPROJECT("SUBPROJECTLIST"), GETSCENARIOS("SCENARIOLIST"), BASELINE_TYPE("baselineType"), REPORT_STATUS("reportStatus"), ERROR_MSG("errMsg"), CHECK_RULE("checkRule");
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

  public NetStormConnectionManager(String URLConnectionString, String username, Secret password, boolean durationPhase, int timeout)
  {
    logger.log(Level.INFO, "NetstormConnectionManger constructor called with parameters with username:{0}", new Object[]{username});

    this.URLConnectionString = URLConnectionString;
    this.username = username;
    this.password = password;
    this.durationPhase = durationPhase;
    this.timeout = timeout;
  }

  public NetStormConnectionManager(String URLConnectionString, String username, Secret password, String project, String subProject, String scenario, String testMode, String baselineType, String pollInterval, String profile,String hiddenBox, boolean generateReport, boolean doNotWaitforTestCompletion, String... gitPull)
  {
    logger.log(Level.INFO, "NetstormConnectionManger constructor called with parameters with username:{0}, project:{2}, subProject:{3}, scenario:{4}, baselineTR:{5}", new Object[]{username, project, subProject, scenario, baselineType});
    logger.log(Level.INFO, "Gitpull - ",gitPull.length);
    logger.log(Level.INFO, "profile - ",profile);
    logger.log(Level.INFO, "NetStormConnectionManager: profile - ",hiddenBox + ", doNotWaitForTestCompletio = " + doNotWaitforTestCompletion);
    this.URLConnectionString = URLConnectionString;
    this.username = username;
    this.project = project;
    this.subProject = subProject;
    this.scenario = scenario;
    this.testMode = testMode;
    this.baselineType = baselineType;
    this.password = password;
    this.pollInterval = Long.parseLong(pollInterval);
    this.profile = profile;
    this.hiddenBox = hiddenBox;
    this.gitPull = (gitPull.length > 0) ? gitPull[0] : "false";
    this.generateReport = generateReport;
    this.doNotWaitforTestCompletion = doNotWaitforTestCompletion;
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

public JSONObject checkGitConfiguration(String repoPath,String username,String password,String passPhrase){
		try
        {
			logger.log(Level.INFO, "checkGitConfiguration method called. with password ="+password+",passPhrase ="+passPhrase);
  	 JSONObject reqObj = new JSONObject();
  	 
     /*Encrypting password and passphrase*/  	
  	  URL urll ;
	  String strr = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
	  urll = new URL(strr+"/ProductUI/productSummary/jenkinsService/getEncryptedStr?password="+password+"&passPhrase="+passPhrase);
	   try{
  	HttpURLConnection connectt = (HttpURLConnection) urll.openConnection();
  	connectt.setConnectTimeout(POLL_CONN_TIMEOUT);
  	connectt.setReadTimeout(POLL_CONN_TIMEOUT);
  	connectt.setRequestMethod("GET");
  	connectt.setRequestProperty("Accept", "application/json");    

  	if (connectt.getResponseCode() != 200) {
  	  logger.log(Level.INFO, "Getting Error code on encrypting  = " + connectt.getResponseCode());
  	}

  	BufferedReader brr = new BufferedReader(new InputStreamReader(connectt.getInputStream()));
  	String encryptRes = brr.readLine();
  	
  	  
  	  logger.log(Level.INFO, "encryptRes = " + encryptRes);
  	logger.log(Level.INFO, "br.readLine() = " + brr.readLine());
  	 JSONObject encResponseObj = (JSONObject) JSONSerializer.toJSON(encryptRes);
  	logger.log(Level.INFO, "encryptRes object = " + encResponseObj);
  	password = encResponseObj.get("password").toString();
  	passPhrase = encResponseObj.get("passPhrase").toString();
    }catch(Exception e){
    	logger.log(Level.SEVERE, "Unknown exception in encrypting password and passphrase.", e);
	}
	   
       logger.log(Level.INFO, "Encrypted password -> "+password+", passPhrase ="+passPhrase);
  	 
  	 /*Testing git configuration*/
	  reqObj.put("productType", "NS");
	  reqObj.put("userName", this.username);
	  	 
	 // String testString = "GIT_HOST = ";
	 // testString = testString + repoIp+" "+repoPort+" "+repoPath+" "+username+" "+password+" true "+passPhrase+" NA "+protocol+" NA";
	 // logger.log(Level.INFO, "testString ="+testString);
	 // reqObj.put("testString", testString);
	  reqObj.put("repoUrl", repoPath);
	  reqObj.put("u", username);
	  reqObj.put("p", "NA");
	  reqObj.put("passType", "0");
	  reqObj.put("passValue", password);
	  reqObj.put("proxyIpPort", "NA");
	  reqObj.put("validateSshkey", "false");
	  logger.log(Level.INFO, "reqObj = "+  reqObj);
	  	 
  	  URL url ;
  	  String response="";
  	  JSONObject finalRes = new JSONObject();
  	  String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
  	  url = new URL(str+"/ProductUI/productSummary/gitRepositoryService/fetchBranch");
  	  
  	     
  	  logger.log(Level.INFO, "versionControl method called. with arguments url = "+  url);
  	  HttpURLConnection conn = (HttpURLConnection) url.openConnection();
  	  conn.setRequestMethod("POST");
  	  conn.setRequestProperty("Content-Type", "application/json");
  	  conn.setRequestProperty("Accept", "application/json");
  	  conn.setDoOutput(true);
  	  String json =reqObj.toString();
  	  OutputStream os = conn.getOutputStream();
  	  os.write(json.getBytes());
  	  os.flush();

  	   if (conn.getResponseCode() != 200) {
     	        throw new RuntimeException("Failed in checkGitConfiguration : HTTP error code : "+ conn.getResponseCode());
  	   }
  	   else 
  	   {
  		  BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));
  	      String output = br.readLine();
  	      try
  	      {
  	    	org.json.simple.JSONObject jsonObj = (org.json.simple.JSONObject)new JSONParser().parse(output);
  	       
  	        String status = (String)jsonObj.get("status");
  	        
  	        logger.log(Level.INFO, "status = "+  status);
  	        
	  	 	if(status != null && !status.isEmpty()){
	  	 		if(status.equals("pass")) {
	  	 			finalRes.put("msg", "Git Configuration passed.");
		 			finalRes.put("errMsg", "");
	  	 		} else {
	  	 			String mssg = (String)jsonObj.get("data");
	  	 			finalRes.put("msg", mssg);
		 			finalRes.put("errMsg", mssg);
	  	 		}
	  	 			
	  	 }
	  	 	else{
	  	 		logger.log(Level.INFO, "response is null ...");
	  	 		finalRes = new JSONObject();
	  	 	}
  	      }
  	      catch(Exception e)
  	      {
  	        // TODO Auto-generated catch block
  	        e.printStackTrace();
  	      }
  	   }
  	 return finalRes;
  	      
  	} catch (MalformedURLException e) {
  	      logger.log(Level.SEVERE, "Unknown exception in checking configuration. MalformedURLException -", e);
  	      return null;
  	}catch (IOException e) {
	      logger.log(Level.SEVERE, "Unknown exception in checking configuration. IOException -", e);
	      return null;
	} catch (Exception e) {
	      logger.log(Level.SEVERE, "Unknown exception in checking configuration.", e);
	      return (null);
	}
  }

public String saveGitConfiguration(String repoPath,String username,String password,String passPhrase){
	try{
		
		/*Encrypting password and passphrase*/  	
	  	  URL urll ;
		  String strr = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
		  urll = new URL(strr+"/ProductUI/productSummary/jenkinsService/getEncryptedStr?password="+password+"&passPhrase="+passPhrase);
		try{
	  	HttpURLConnection connectt = (HttpURLConnection) urll.openConnection();
	  	connectt.setConnectTimeout(POLL_CONN_TIMEOUT);
	  	connectt.setReadTimeout(POLL_CONN_TIMEOUT);
	  	connectt.setRequestMethod("GET");
	  	connectt.setRequestProperty("Accept", "application/json");    

	  	if (connectt.getResponseCode() != 200) {
	  	  logger.log(Level.INFO, "saveGitConfiguration: Getting Error code while encrypting  = " + connectt.getResponseCode());
	  	}

	  	BufferedReader brr = new BufferedReader(new InputStreamReader(connectt.getInputStream()));
	  	String encryptRes = brr.readLine();
	  	
	  	  
	  	  logger.log(Level.INFO, "saveGitConfiguration: encryptRes = " + encryptRes);
	  	 JSONObject encResponseObj = (JSONObject) JSONSerializer.toJSON(encryptRes);
	  	logger.log(Level.INFO, "saveGitConfiguration: encryptRes object = " + encResponseObj);
	  	password = encResponseObj.get("password").toString();
	  	passPhrase = encResponseObj.get("passPhrase").toString();
		}catch(Exception e){
			logger.log(Level.SEVERE, "saveGitConfiguration: Unknown exception in encrypting password and passphrase.", e);
		}
	    logger.log(Level.INFO, "saveGitConfiguration: Encrypted password -> "+password+", passPhrase ="+passPhrase);
	    
	    /*Saving git configuration*/
	    JSONObject saveParam = new JSONObject();
	    saveParam.put("GIT_HOST_branch", "main");
	    saveParam.put("GIT_HOST_email", "NA");
	    saveParam.put("GIT_HOST_fname", username);
	    saveParam.put("GIT_HOST_enable", "true");
	    saveParam.put("GIT_HOST_ip", "");
	    saveParam.put("GIT_HOST_ssl", "true");
	    saveParam.put("GIT_HOST_isSSLCertificateDisable", "false");
	    saveParam.put("GIT_HOST_pass_phrase", passPhrase);
	    saveParam.put("GIT_HOST_pwd", password);
	    saveParam.put("GIT_HOST_url", repoPath);
	    saveParam.put("GIT_HOST_uname", username);
	    saveParam.put("SSH_KEY", "0");
	    saveParam.put("VERSION_CONTROL", "1");
	    saveParam.put("git_author", "");
	    saveParam.put("git_email", "");
	    saveParam.put("git_proxy", "");
	    saveParam.put("git_url", repoPath);
	    saveParam.put("git_user", username);
	    saveParam.put("http_proxy", "");
	    saveParam.put("https_proxy", "");
	    saveParam.put("isSaveForFutureProfile", true);
	    saveParam.put("operationMode", "0");
	    saveParam.put("passphrase", passPhrase);
	    saveParam.put("ssh_proxy", "");
	    saveParam.put("useproxy", "false");
	    saveParam.put("validateSshkey", "false");
	    
	    logger.log(Level.INFO, "saveGitConfiguration: saveParam = "+  saveParam);
	    
	    StringBuilder result = new StringBuilder();
	    URL url = new URL(strr+"/DashboardServer/web/commons/setGitConfiguration?userName=" + this.username);
	    logger.log(Level.INFO, "saveGitConfiguration: url = "+  url);
	    
	    HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("POST");
	  	  conn.setRequestProperty("Content-Type", "application/json");
	  	  conn.setRequestProperty("Accept", "application/json");
	  	  conn.setDoOutput(true);
	  	  String json =saveParam.toString();
	  	  OutputStream os = conn.getOutputStream();
	  	  os.write(json.getBytes());
	  	  os.flush();
	  	  
	  	if (conn.getResponseCode() != 200) {
 	        throw new RuntimeException("Failed in saveGitConfiguration : HTTP error code : "+ conn.getResponseCode());
	  	}
	       BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
	       String line;
	       while ((line = br.readLine()) != null) {
	         result.append(line);
	       }
	       br.close();
	       return result.toString();	    
	}catch(Exception e){
		logger.log(Level.SEVERE, "Unknown exception in saveGitConfiguration method.", e);
	    return (null);
	}
}

public String getGitConfiguration(){
	try{
		logger.log(Level.INFO, "getGitConfiguration: Method called.");
		URL urll ;
		String resMsg="";
		StringBuffer errMsg = new StringBuffer();
		JSONObject resObj = new JSONObject();
		String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
		urll = new URL(str+"/DashboardServer/web/commons/getGitConfiguration?userName=" + username+  "&activeProfile=");
		try{
		  	HttpURLConnection connectt = (HttpURLConnection) urll.openConnection();
		  	connectt.setConnectTimeout(POLL_CONN_TIMEOUT);
		  	connectt.setReadTimeout(POLL_CONN_TIMEOUT);
		  	connectt.setRequestMethod("GET");
//		  	connectt.setRequestProperty("Accept", "application/json");    

		  	if (connectt.getResponseCode() != 200) {
		  	  logger.log(Level.INFO, "getGitConfiguration: Getting Error code while checking for git config  = " + connectt.getResponseCode());
		  	}

		  	BufferedReader brr = new BufferedReader(new InputStreamReader(connectt.getInputStream()));
		  	resMsg = brr.readLine();
		  	logger.log(Level.INFO, "getGitConfiguration: resMsg -"+resMsg);
		  	
			}catch(Exception e){
				logger.log(Level.SEVERE, "getGitConfiguration :Unknown exception in checking if git is configured -", e);
			}
		return resMsg;
	}catch(Exception e){
		logger.log(Level.SEVERE, "getGitConfiguration sec :Unknown exception in checking if git is configured -", e);
		return null;
	}
}

public String pullObjectsFromGit(){
	try{
		logger.log(Level.INFO, "pullObjectsFromGit called...");
		/*Checking if git is already configured*/
		String res="";
		String response = "";
		//JSONObject resObj = new JSONObject();
		String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
		String resMsg = getGitConfiguration();
		try{
	  	logger.log(Level.INFO, "pullObjectsFromGit: resMsg -"+resMsg);
	  	if(resMsg == null||resMsg.equals("")||resMsg.equals("notConfigured")){
	  		logger.log(Level.INFO, "Git is not configured ...");
	  		res = "Git configuration is unavailable. Configure git repository first";
	  		response = res;
	  		return res;
	  	}
		}catch(Exception e){
			logger.log(Level.SEVERE, "Unknown exception in checking if git is configured -", e);
		}
	  	
		logger.log(Level.INFO, "Going to pull objects ...");
		String gitResponse[] = resMsg.split(" ");
		String repoUrl = "";
		if(gitResponse != null && gitResponse.length > 0) {
			repoUrl = gitResponse[0];
		}
	  	/*If git is configured, pull/clone scenario, scripts, testcases, checkprofiles and testsuites from configured repo*/
		JSONObject obj = new JSONObject();
		obj.put("masterRepo", repoUrl);
		obj.put("passType", "0");
		obj.put("passValue", "");
		obj.put("productType", "NS");
		obj.put("repoProject", "configuration");
		obj.put("userName", this.username);
		
		logger.log(Level.INFO, "clone request object = " + obj);
		URL url ;
		url = new URL(str+"/ProductUI/productSummary/gitRepositoryService/gitClone");
		
	  	HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	  	conn.setRequestMethod("POST");  
	  	conn.setRequestProperty("Content-Type", "application/json");
	  	conn.setRequestProperty("Accept", "application/json");
	  	conn.setDoOutput(true);
	  	String json =obj.toString();
	  	OutputStream os = conn.getOutputStream();
	  	os.write(json.getBytes());
	  	os.flush();

	  	if (conn.getResponseCode() != 200) {
	  	  logger.log(Level.INFO, "Getting Error code while git clonning  = " + conn.getResponseCode());
	  	}
	  	BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));
	  	String cloneRes = br.readLine();
	  	logger.log(Level.INFO, "cloneRes -"+cloneRes);
	  	
	  	/*Now after clonning, doing git pull*/
	  	JSONObject objreq = new JSONObject();
		objreq.put("activeProfile", "");
		objreq.put("passType", "0");
		objreq.put("passValue", "");
		objreq.put("productType", "NS");
		objreq.put("repo", "configuration");
		objreq.put("userName", this.username);
		
		logger.log(Level.INFO, "pull request object = " + objreq);
		url = new URL(str+"/ProductUI/productSummary/gitRepositoryService/gitRefresh");
		
	  	HttpURLConnection urlconn = (HttpURLConnection) url.openConnection();
	  	urlconn.setRequestMethod("POST");  
	  	urlconn.setRequestProperty("Content-Type", "application/json");
	  	urlconn.setRequestProperty("Accept", "application/json");
	  	urlconn.setDoOutput(true);
	  	String data =objreq.toString();
	  	OutputStream os1 = urlconn.getOutputStream();
	  	os1.write(data.getBytes());
	  	os1.flush();

	  	if (urlconn.getResponseCode() != 200) {
	  	  logger.log(Level.INFO, "Getting Error code while git clonning  = " + urlconn.getResponseCode());
	  	}
	  	BufferedReader brreader = new BufferedReader(new InputStreamReader(urlconn.getInputStream()));
	    response = brreader.readLine();
		
	    logger.log(Level.INFO, "Git Pull response = " + response);
	  	return response;
	}catch(Exception e){
		logger.log(Level.SEVERE, "Unknown exception in pullObjectsFromGit method.", e);
	    return (null);
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
      jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), getUrlString());
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.START_TEST.toString());
      jsonRequest.put(JSONKeys.PROJECT.getValue(), project);
      jsonRequest.put(JSONKeys.SUBPROJECT.getValue(), subProject);
      jsonRequest.put(JSONKeys.SCENARIO.getValue(), scenario);
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE);
      jsonRequest.put(JSONKeys.TESTMODE.getValue(), testMode);
      jsonRequest.put(JSONKeys.REPORT_STATUS.getValue(), ""); 
      jsonRequest.put(JSONKeys.BASELINE_TYPE.getValue(),baselineType);
      jsonRequest.put("workProfile",profile);
      jsonRequest.put("scriptHeaders",hiddenBox);
      jsonRequest.put("generateReport", Boolean.toString(generateReport));
         
      
//      if(getBaselineTR() != null && !getBaselineTR().trim().equals(""))
//       {
//         String baseline = getBaselineTR();
//         if(baseline.startsWith("TR"))
//	  baseline = baseline.substring(2, baseline.length());
//	  
//         jsonRequest.put("BASELINE_TR", baseline);
//       } 
//      else
//       jsonRequest.put("BASELINE_TR", "-1");
       

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

      if(getRampUpDuration() != null && !getRampUpDuration().trim().equals("")){
    	  jsonRequest.put("RAMP_UP_DURATION", getRampUpDuration());
      }
      
      if(gettName()!= null && !gettName().trim().equals(""))
      {
        jsonRequest.put("TNAME", gettName());
      }
      if(getAutoScript()!= null && !getAutoScript().trim().equals(""))
      {
        jsonRequest.put("AUTOSCRIPT", getAutoScript());
      }
      
      if(getEmailIdTo()!= null && !getEmailIdTo().trim().equals(""))
      {
        jsonRequest.put("EmailIdTo", getEmailIdTo());
      }
      
      if(getEmailIdCc()!= null && !getEmailIdCc().trim().equals(""))
      {
        jsonRequest.put("EmailIdCc", getEmailIdCc());
      }
      
      if(getEmailIdBcc()!= null && !getEmailIdBcc().trim().equals(""))
      {
        jsonRequest.put("EmailIdBcc", getEmailIdBcc());
      }
      
     logger.log(Level.INFO, "getDataDir = " + getDataDir());
      if(getDataDir() != null && !getDataDir().trim().equals(""))
      {
    	  logger.log(Level.INFO, "inside check");
        jsonRequest.put("DataDir", getDataDir());
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
    else if(type.equals("START_MULTIPLE_TEST")) {
    	JSONArray testsuiteArray = new JSONArray();
		for(int i=0; i < testsuitelist.size(); i++) {
			String prefix[] = testsuitelist.get(i).split("_");
			if(prefix.length > 1) {
				logger.log(Level.INFO, "parameter dto = " + testsuiteParameterDTO.get(prefix[0]));
			 JSONObject obj = testsuiteParameterDTO.get(prefix[0]).testsuiteJson();
			 obj.put("scenario", testsuitelist.get(i));
			 obj.put("project", project);
			 obj.put("subproject", subProject);
			 obj.put("testmode", testMode);
			 obj.put("scriptHeaders",hiddenBox);
			 obj.put("baselineType", baselineType);
			 testsuiteArray.add(obj);
			}
		}
		
		jsonRequest.put(JSONKeys.USERNAME.getValue(), username);
	    jsonRequest.put(JSONKeys.PASSWORD.getValue(), password.getPlainText()); 
	    jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), URLConnectionString);
		jsonRequest.put("scenario", testsuiteArray);
		jsonRequest.put("workProfile",profile);
	    jsonRequest.put("generateReport", Boolean.toString(generateReport));
	    String uniqueID = UUID.randomUUID().toString();
	    job_id = uniqueID;
	    jsonRequest.put("JOB_ID", uniqueID);
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
      jsonRequest.put("workProfile",profile);
    }
    else if(type.equals("GET_SUBPROJECT"))
    {
      jsonRequest.put(JSONKeys.USERNAME.getValue(), username);
      jsonRequest.put(JSONKeys.PASSWORD.getValue(), password.getPlainText());
      jsonRequest.put(JSONKeys.PROJECT.getValue(), project);
      jsonRequest.put(JSONKeys.OPERATION_TYPE.getValue(), OperationType.GETSUBPROJECT.toString());
      jsonRequest.put(JSONKeys.STATUS.getValue(), Boolean.FALSE);
      jsonRequest.put(JSONKeys.URLCONNECTION.getValue(), URLConnectionString); 
      jsonRequest.put("workProfile",profile);
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
      jsonRequest.put("workProfile",profile);
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
   
  public ArrayList<String> getProfileList(StringBuffer errMsg)
  {
	 try {
		 logger.log(Level.INFO, " getting profile list ");
		 ArrayList<String> profiles = new ArrayList<String>();
		 URL url ;
         String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
	      url = new URL(str+"/DashboardServer/web/commons/getProfileList?userName=" + username);
	     
	      logger.log(Level.INFO, "getProfileList url-"+  url);
	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
	      conn.setRequestProperty("Accept-Encoding", "gzip, deflate");
	      conn.setRequestProperty("Content-Type","application/json");
	      if(conn.getResponseCode()!= 200) {
	    	  logger.log(Level.INFO, "getting error in fetching profile list.");
	    	  profiles = new ArrayList<String>();
	    	  return profiles;
	      }
	      
	      //BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
	      BufferedReader br = new BufferedReader(new InputStreamReader(new GZIPInputStream(conn.getInputStream())));
	      StringBuilder stb = new StringBuilder();
	      
	      String response = null;
	      while((response = br.readLine())!= null) {
	    	  stb.append(response);
	      }
	      logger.log(Level.INFO, "response- " + stb.toString());
	      JSONArray arr = JSONArray.fromObject(stb.toString());
	      //JSONArray arr = (JSONArray) JSONSerializer.toJSON(response);
	      for(Object obj:arr) {
	    	  String res = JSONObject.fromObject(obj).get("profileName").toString();
	    	  profiles.add(res);
	    	  logger.log(Level.INFO, "res- " + res);
	      }
	      logger.log(Level.INFO, "profiles- " + profiles.size());
	      return profiles;
	 }
	  
	  catch(Exception e){
		  logger.log(Level.SEVERE, "Error getting profile list- " + e);
		  return null;
	  }
  }
  
  
   
  
  public ArrayList<String> getProjectList(StringBuffer errMsg,String activeProfile)
  {
    logger.log(Level.INFO, "getProjectList method called.");
    logger.log(Level.INFO, "activeProfile -"+activeProfile);
    
    try
    {
      logger.log(Level.INFO, "Making connection to Netstorm with following request uri- " + URLConnectionString);
      logger.log(Level.INFO, "Sending requets to get project list - " + URLConnectionString);
      JSONObject jsonResponse  = null;
      this.profile = activeProfile;
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

  public ArrayList<String> getSubProjectList(StringBuffer errMsg , String project, String activeProfile)
  {
    logger.log(Level.INFO, "getSubProjectList method called.");
    
    try
    {
      logger.log(Level.INFO, "Making connection to Netstorm with following request uri- " + URLConnectionString);

      this.project =  project;
      this.profile = activeProfile;
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
  
  public ArrayList<String> getScenarioList(StringBuffer errMsg , String project, String subProject, String mode, String activeProfile)
  {
    logger.log(Level.INFO, "getScenarioList method called.");
    try
    {
      logger.log(Level.INFO, "Making connection to Netstorm with following request uri- " + URLConnectionString);
      this.project = project;
      this.subProject = subProject;
      this.testMode = mode;
      this.profile = activeProfile;
   
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

  public HashMap startNetstormTest(StringBuffer errMsg , PrintStream consoleLogger, String repoPath)
  {
	  logger.log(Level.INFO, "startNetstormTest() called.");
	  
	  logger.log(Level.INFO, "startNetstormTest: hiddenBox -"+hiddenBox);
      testRun = -1;
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

                 // logger.log(Level.INFO, "json object" + jsonRequest);
		  try{
			if(gitPull.equals("true")){
			  logger.log(Level.INFO, "Going to pull from GIT...");
			  consoleLogger.println("Starting Git pull ... ");
			  String res = pullObjectsFromGit();
//			  if(res != null && !res.isEmpty()){
//				  logger.log(Level.INFO, "res -"+res);
//			  }else{
//	            consoleLogger.println("GIT Pull was unsuccessful.");
//	          }
			  if(res != null && !res.isEmpty()){
	             consoleLogger.println(res);
	           
	           }else{
	             consoleLogger.println("GIT Pull was unsuccessful.");
	          }
		  	}
		  }catch(Exception ex){
			  logger.log(Level.SEVERE, "Exception in pulling from Git -", ex);
		  }

		  try {
			  URL url;
			  String str =   getUrlString();//URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
              logger.log(Level.INFO, "url str = "+ str);
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

			  //consoleLogger.println("Test is Started Successfully. Now waiting for Test to End ...");

			  BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
			  setResult(br.readLine());

			//  logger.log(Level.INFO, "RESPONSE for metric startNetstormTest  -> " + this.result.toString());
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

			  logger.log(Level.INFO, "testrun before polling = " + testRun);
			  /*Starting Thread and polling to server till test end.*/
			  connectNSAndPollTestRun();
			 // consoleLogger.println("Getting Netstorm Report. It may take some time. Please wait...");

			  /*Setting TestRun here.*/
			  jsonResponse.put("TESTRUN", testRun + "" );
			  jsonResponse.put("REPORT_STATUS", "");

			  if(testMode.equals("T"))
			    consoleLogger.println("Test Cycle Number - "+testCycleNum);
			 
			  if(testRun == -1)
			  {

				  logger.log(Level.INFO, "Test is Failed .");
				  if(!scenario.equals("") && scenario != null && !scenario.equals("---Select Scenarios ---")){
        			  if(testMode.equals("N"))
        				  consoleLogger.println("Test is either not started or failed due to some error in the scenario.");
        			  else
        				  consoleLogger.println("Test is either not started or failed due to some error in the scenario. The test suite execution ended with status 'NetStorm Fail'.");
            	  }else{
				  consoleLogger.println("Test is either not started or failed due to some reason");
            	  }
                  return resultMap;            
			  }

			  /** if check rule file is imported then call this method. */
			  if(this.jkRule != null) {
				  createCheckRuleFile(str);
			  }
			  
			  if(doNotWaitforTestCompletion == false)
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
  
  public HashMap startMultipleTest(StringBuffer errMsg , PrintStream consoleLogger, String repoPath)
  {
	  logger.log(Level.INFO, "startNetstormTest() called.");
	  
	  logger.log(Level.INFO, "startNetstormTest: hiddenBox -"+hiddenBox);

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
		  JSONObject jsonRequest = new JSONObject();

			   jsonRequest = makeRequestObject("START_MULTIPLE_TEST");
		  consoleLogger.println("Job Id = " + job_id);
		  consoleLogger.println("Starting Test ... ");

                 // logger.log(Level.INFO, "json object" + jsonRequest);
		  try{
			if(gitPull.equals("true")){
			  logger.log(Level.INFO, "Going to pull from GIT...");
			  consoleLogger.println("Starting Git pull ... ");
			  String res = pullObjectsFromGit();
//			  if(res != null && !res.isEmpty()){
//				  logger.log(Level.INFO, "res -"+res);
//			  }else{
//	            consoleLogger.println("GIT Pull was unsuccessful.");
//	          }
			  if(res != null && !res.isEmpty()){
				  consoleLogger.println(res);
			  }else{
				  consoleLogger.println("GIT Pull was unsuccessful.");
			  }
		  	}
		  }catch(Exception ex){
			  logger.log(Level.SEVERE, "Exception in pulling from Git -", ex);
		  }

		  try {
			  URL url;
			  String str =   getUrlString();//URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));

			  logger.log(Level.INFO, "this.jkRule- " + this.jkRule);    
			  url = new URL(str+"/ProductUI/productSummary/jenkinsService/startMultipleTest");

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

			  //consoleLogger.println("Test is Started Successfully. Now waiting for Test to End ...");

			  BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
			  setResult(br.readLine());

			//  logger.log(Level.INFO, "RESPONSE for metric startNetstormTest  -> " + this.result.toString());
			  jsonResponse = (JSONObject) JSONSerializer.toJSON(this.result);

			  logger.log(Level.INFO, jsonResponse.toString());
			  /*Getting scenario name from server.*/
//			  JSONArray testsuiteArr = (JSONArray)jsonResponse.get("scenario");
//			  if(testsuiteArr.size() > 0) {
//				  JSONObject obj = (JSONObject)testsuiteArr.get(0);
//				  if (jsonResponse.containsKey("scenario")) {
//					  scenario = jsonResponse.get("scenario").toString();
//				  }
//			  }
			  String portStr = getUrlString();
			  new BuildActionStopThread(job_id,username,portStr);
			  

			  /*Here checking the scenario Name of server.*/
//			  if (scenarioName == null || scenarioName.equals("NA")) {
//				  consoleLogger.println("Getting Empty Response from server. Something went wrong.");     
//			  }

			  logger.log(Level.INFO, "Here starting the thread for checking the running scenario of server.");

			  /*Creating URL for polling.*/
			  pollURL = str + "/ProductUI/productSummary/jenkinsService/checkTestStatus";

			  /*Starting Thread and polling to server till test end.*/
			  connectNSPollTestRun();
			 // consoleLogger.println("Getting Netstorm Report. It may take some time. Please wait...");

			  /*Setting TestRun here.*/
			  jsonResponse.put("TESTRUN", testRun + "" );
			  jsonResponse.put("REPORT_STATUS", "");
			  
			  if(!this.errMsg.isEmpty())
			    jsonResponse.put("errMsg", this.errMsg);

			  if(testMode.equals("T"))
			    consoleLogger.println("Test Cycle Number - "+testCycleNum);
			  
			  if(testRun == -1)
			  {

				  logger.log(Level.INFO, "Test is Failed .");
				  if(!scenario.equals("") && scenario != null && !scenario.equals("---Select Scenarios ---")){
        			  if(testMode.equals("N"))
        				  consoleLogger.println("Test is either not started or failed due to some error in the scenario.");
        			  else
        				  consoleLogger.println("Test is either not started or failed due to some error in the scenario. The test suite execution ended with status 'NetStorm Fail'.");
            	  }else{
				  consoleLogger.println("Test is either not started or failed due to some reason");
            	  }
                  return resultMap;            
			  }

			  /** if check rule file is imported then call this method. */
			  if(this.jkRule != null) {
				  createCheckRuleFile(str);
			  }
			  
			  if(doNotWaitforTestCompletion == false)
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
  
  
  
  public void checkTestSuiteStatus(PrintStream consoleLoger, FilePath fp, Run build) {
	  try {
		  consoleLogger.println("Checking for testsuite execution status.");
		  String str =   getUrlString();
		  String url = str + "/ProductUI/productSummary/jenkinsService/checkTestSuiteStatus";
	      /* Creating the thread. */
	      Runnable pollTestRunState = new Runnable()
	      {
	        public void run()
	        {
	          try {

	            /*Keeping flag based on TestRun status on server.*/
	            boolean isTestSuiteRunning = true;
	            
	            /*Initial Sleep Before Polling.*/
	            try {
	              
	              /*Delay to poll due to test is taking time to start.*/
	              Thread.sleep(30 * 1000);     
	              
	            } catch (Exception ex) {
	              logger.log(Level.SEVERE, "Error in initial sleep before polling.", ex);
	              build.setResult(Result.UNSTABLE);
	            }

	            logger.log(Level.INFO, "Starting Polling to server.");
	            
	            /*Running Thread till test stopped.*/
	            while (isTestSuiteRunning) {
	              try {
	        	
	        	/*Creating Polling URL.*/
	        	String pollURLWithArgs = url + "?testCycleNumber=" + testCycleNum + "&testRun=" + testRun;    	
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
	        	      	  
	        	  
	        	  String mssg = "";
	        	  if(pollResponse.getString("mssg") != null && !pollResponse.getString("mssg").equals("")) {
	        		 mssg = pollResponse.getString("mssg");
	        		 consoleLoger.println(pollResponse.getString("mssg"));
	        	  }
	        	  
	        	  if(pollResponse.getBoolean("status")) {
	        	    /*Terminating Loop when test is stopped.*/
	        	    isTestSuiteRunning = false;
	        	  }
	        	  
	        	  if(mssg.startsWith("TestSuite is Executed Successfully.")) {
	        		  
	        		  if(pollResponse.has("Report Status")) {
	        			  String status = pollResponse.getString("Report Status");
	        			  consoleLoger.println("Report Status = " + status);
	        			  if(status.trim().equals("PASS"))
	        				 build.setResult(Result.SUCCESS);
	        			  else if(status.trim().equals("FAIL"))
	        				 build.setResult(Result.FAILURE);
	        			  
	        		  }
	        		  boolean htmlReport = getHTMLReport(fp);
	        		  if(htmlReport == false)
	        			 consoleLoger.println("Error in fetching report from server.");
	        		  else {
	        			  consoleLoger.println("Successfully fetch the HTML report from server. Now fetching Pdf file.");
	        			boolean isPdfUploaded = dumpPdfInWorkspace(fp);
	        			if(isPdfUploaded) {
	        				consoleLoger.println("Pdf File is Uploaded succesfully.");
	        			} else
	        				consoleLoger.println("Error in Uploading Pdf File.");
	        		  }
	        	  } else if(mssg.startsWith("TestSuite is Executed Completely.")) {
	        		  build.setResult(Result.UNSTABLE);
	        	  
	        	  }
	        	  
	        	  
	                  
	        	} catch (Exception e) {
	        	  logger.log(Level.SEVERE, "Error in parsing polling response = " + pollResString, e);
	        	  build.setResult(Result.UNSTABLE);
	        	}

	        	/*Closing Stream.*/
	        	try {
	        	  br.close();
	        	} catch (Exception e) {
	        	  logger.log(Level.SEVERE, "Error in closing stream inside polling thread.", e);
	        	  build.setResult(Result.UNSTABLE);
	        	}
	              } catch (Exception e) {
	                logger.log(Level.SEVERE, "Error in polling running testcycleNumber with interval. Retrying after 5 sec.", e);
	                build.setResult(Result.UNSTABLE);
	              }

	              /*Repeating till Test Ended.*/
	              try {
	        //    consoleLogger.println("Test in progress. Going to check on server. Time = " + new Date() + ", pollInterval = " + pollInterval);
	        	
//	            if(testRun > 0)
//	              pollInterval  = 60;
	            
	            if(isTestSuiteRunning == true)
	              Thread.sleep(30 * 1000);
	                
	              } catch (Exception ex) {       	
	        	logger.log(Level.SEVERE, "Error in polling connection in loop", ex);
	        	build.setResult(Result.UNSTABLE);
	              } 
	            }

	          } catch (Exception e) {
	            logger.log(Level.SEVERE, "Error in polling running testRun with interval.", e);
	            build.setResult(Result.UNSTABLE);
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
	      
	     // consoleLogger.println("TestSuite Execution is Completed. Now fetching report from server.");
	  } catch(Exception e) {
		  logger.log(Level.SEVERE, "Unknown exception in establishing connection.", e);
	  }
  }
  
  /*Method to dump pdf file in workspace*/
  private boolean dumpPdfInWorkspace(FilePath fp) throws IOException, InterruptedException {
	  /*getting testrun number*/
	  String testRun = NetStormBuilder.testRunNumber;
	  /*path of directory i.e. /var/lib/jenkins/workspace/jobName*/
	  String dir = fp + "/TR" + testRun;
	  
	  
	  logger.log(Level.INFO, "Pdf directory"+dir);
	  
	  
	   FilePath fz = new FilePath(fp.getChannel(), fp + "/TR" + testRun);
	   fz.mkdirs();
	   FilePath fk = new FilePath(fp.getChannel(), fz + "/testsuite_report_" + testRun + ".pdf");
	  logger.log(Level.INFO, "File path for pdf file = " + fk);
	  
	  try {
		  URL urlForPdf;
		  String str =   getUrlString();
		  urlForPdf = new URL(str+"/ProductUI/productSummary/jenkinsService/getPdfData");
		  logger.log(Level.INFO, "urlForPdf-"+urlForPdf);

		  HttpURLConnection connect = (HttpURLConnection) urlForPdf.openConnection();
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
			//  FileOutputStream fos = new FileOutputStream(fp);
			  BufferedOutputStream bos = new BufferedOutputStream(fk.write());
			  int bytesRead;
			  while((bytesRead = is.read(mybytearray)) > 0){
				logger.log(Level.INFO, "bytesRead inside while check"+ bytesRead);
				bos.write(mybytearray, 0, bytesRead);
				//fp.write(content, null);
			  }
			  bos.close();
			  is.close();
		  } else {
			  logger.log(Level.INFO, "ErrorCode-"+ connect.getResponseCode());
			  logger.log(Level.INFO, "content type-" + connect.getContentType());
			  return false;
		  }
		  return true;
	  } catch (Exception e){
		  logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
		  return false;
	  }

  }  
  
  private boolean getHTMLReport(FilePath fp) throws IOException, InterruptedException {
 	 
	  /*getting testrun number*/
	  String testRun = NetStormBuilder.testRunNumber;
	  /*path of directory i.e. /var/lib/jenkins/workspace/jobName*/
	  String zipFile = fp + "/TestSuiteReport.zip";

   	 FilePath  fz = new FilePath(fp.getChannel(), zipFile);
   	 
   	
   	   if(fz.exists()) {
 		   fz.delete();
 		   fz = new FilePath(fp.getChannel(), zipFile);
   	   }
	  
	  try {
		  JSONObject jsonRequest = new JSONObject();
		  jsonRequest.put("testRun", testRun);
		  jsonRequest.put("isNDE", false);
		  
		  URL urlForHTMLReport;
		  String str =   getUrlString();
		  urlForHTMLReport = new URL(str+"/ProductUI/productSummary/jenkinsService/getHTMLReport");
		  logger.log(Level.INFO, "urlForPdf-"+urlForHTMLReport);

		  HttpURLConnection connect = (HttpURLConnection) urlForHTMLReport.openConnection();
		  connect.setConnectTimeout(0);
		  connect.setReadTimeout(0);
		  connect.setRequestMethod("POST");
		  connect.setRequestProperty("Content-Type", "application/octet-stream");

		  connect.setDoOutput(true);
		  java.io.OutputStream outStream = connect.getOutputStream();
		  String json =jsonRequest.toString();
		  outStream.write(json.getBytes());
		  outStream.flush();

		  if (connect.getResponseCode() == 200) {
			  logger.log(Level.INFO, "response 200 OK");   
			  byte[] mybytearray = new byte[1024];
			  InputStream is = connect.getInputStream();
			 // FileOutputStream fos = new FileOutputStream(file);
			  BufferedOutputStream bos = new BufferedOutputStream(fz.write());
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
			  return false;
		  }
		  
		  String destDir = fp + "/TestSuiteReport";
		  
		  FilePath dir = new FilePath(fp.getChannel(), destDir);
		  if(dir.exists())
			  dir.deleteRecursive(); 
		  dir.mkdirs();
			 
          unzip(dir, fz);
		  return true;
	  } catch (Exception e){
		  logger.log(Level.SEVERE, "Unknown exception in methid getHTMLreport. IOException -", e);
		  e.printStackTrace();
		  return false;
	  }
	  
  }
  
  private void unzip(FilePath dir, FilePath zipFile) throws IOException, InterruptedException {
	    try {
	    	 logger.log(Level.INFO, "inside unzip method...");
	    	InputStream in = zipFile.read();
	    	dir.unzipFrom(in);
	    } catch (IOException e) {
	    	logger.log(Level.SEVERE, "Exception in unzipping file = " + e);
	        e.printStackTrace();
	    }
	    
	}

   public void createCheckRuleFile(String restUrl) {
	 try {
	   logger.log(Level.INFO, "inside createCheckRuleFile - username :"+username+", profile :"+profile);
	   JSONObject jsonRequest = new JSONObject();
	   jsonRequest.put(JSONKeys.PROJECT.getValue(), project);
	   jsonRequest.put(JSONKeys.SUBPROJECT.getValue(), subProject);
	   jsonRequest.put(JSONKeys.SCENARIO.getValue(), scenario);
	   jsonRequest.put(JSONKeys.TEST_RUN.getValue(), testRun + "");
	   jsonRequest.put(JSONKeys.CHECK_RULE.getValue(), this.jkRule);
	   jsonRequest.put("username", username);
	   jsonRequest.put("profile", profile);
	   
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
      
      //consoleLogger.println("Processing Server Response ....");
      
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
	 // consoleLogger.println(repotStatus); 
	}

	if(jsonResponse.get(JSONKeys.TEST_RUN.getValue()) != null) {
	  String testRun= (String)(jsonResponse.get(JSONKeys.TEST_RUN.getValue()));
	  resultMap.put("STATUS", status);
	  resultMap.put("TESTRUN",testRun);
	  
	  if(testMode.equals("T"))
	    resultMap.put("TEST_CYCLE_NUMBER", testCycleNum);

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
	  
	  if(doNotWaitforTestCompletion == false)
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

      consoleLogger.println("Test Started. Now tracking TestRun based on running scenario.");
      
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
              Thread.sleep(pollInterval * 1000);     
              
            } catch (Exception ex) {
              logger.log(Level.SEVERE, "Error in initial sleep before polling.", ex);
            }

            logger.log(Level.INFO, "Starting Polling to server.");
            
            /*Running Thread till test stopped.*/
            while (isTestRunning) {
              try {
        	
             logger.log(Level.INFO, "testrun= "+ testRun);
        	/*Creating Polling URL.*/
        	String pollURLWithArgs = pollURL + "?proSubProject=" + scenarioName + "&testRun=" + testRun;    	
        	logger.log(Level.INFO, "poll url = "+ pollURLWithArgs);
        	
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
        	    int stopTR = -1;
        	    stopTR = pollResponse.getInt("testRun");
        	    logger.log(Level.INFO, "stopTR = " + stopTR);
        	    String portStr = getUrlString();
        	    new BuildActionStopTest(stopTR,username,portStr);
        	  }
        	  
        	  if(pollResponse.getBoolean("status")) {
        	    /*Terminating Loop when test is stopped.*/
        	    isTestRunning = false;
        	  }
                  
        	  testCycleNum = pollResponse.getString("testCycleNumber");  
        	  
        	 if(doNotWaitforTestCompletion == true) {
        		 if(testRun > 0) {
        			 isTestRunning = false;
        		 }
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
            consoleLogger.println("Test in progress. Going to check on server. Time = " + new Date() + ", pollInterval = " + pollInterval);
        	
            if(testRun > 0)
              pollInterval  = 60;
            
            if(isTestRunning == true)
              Thread.sleep(pollInterval * 1000);
                
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
      
     // consoleLogger.println("TestRun is stopped. Now checking the server state.");
      
    } catch (Exception e) {
      logger.log(Level.SEVERE, "Error in polling running testRun.", e);
    }
    
  }
  
  /**
   * Method is used for checking connection with netstorm and polling netstorm for testrun running status based on scenario name.
   * @param scenarioName
   * @param consoleLogger
   */
  private void connectNSPollTestRun() {
    try {

      consoleLogger.println("Test Started. Now tracking TestRun based on running scenario.");
      
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
              Thread.sleep(pollInterval * 1000);     
              
            } catch (Exception ex) {
              logger.log(Level.SEVERE, "Error in initial sleep before polling.", ex);
            }

            logger.log(Level.INFO, "Starting Polling to server.");
            
            /*Running Thread till test stopped.*/
            while (isTestRunning) {
              try {
        	
        	/*Creating Polling URL.*/
        	String pollURLWithArgs = pollURL + "?JOB_ID=" + job_id + "&testMode=" + testMode + "&testRun=" + testRun + "&testCycleNum=" + testCycleNum;    	
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
        	    int stopTR = -1;
        	    stopTR = pollResponse.getInt("testRun");
        	    logger.log(Level.INFO, "stopTR = " + stopTR);
        	    String portStr = getUrlString();
        	    new BuildActionStopTest(stopTR,username,portStr);
        	  }
        	  
        	  if(pollResponse.getBoolean("status")) {
        	    /*Terminating Loop when test is stopped.*/
        	    isTestRunning = false;
        	  }
               
        	  if(testMode.equals("T") && pollResponse.has("testCycleNumber"))
        	    testCycleNum = pollResponse.getString("testCycleNumber"); 
        	  
        	  if(pollResponse.has("errMsg")) {
        		  errMsg = pollResponse.getString("errMsg");
        		 consoleLogger.println(errMsg); 
        	  }
        	  
        	 if(doNotWaitforTestCompletion == true) {
        		 if(testRun > 0) {
        			 isTestRunning = false;
        		 }
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
            consoleLogger.println("Test in progress. Going to check on server. Time = " + new Date() + ", pollInterval = " + pollInterval);
        	
            if(testRun > 0)
              pollInterval  = 60;
            
            if(isTestRunning == true)
              Thread.sleep(pollInterval * 1000);
                
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
      
     // consoleLogger.println("TestRun is stopped. Now checking the server state.");
      
    } catch (Exception e) {
      logger.log(Level.SEVERE, "Error in polling running testRun.", e);
    }
    
  }


  /**
   * Method is used for checking connection with netstorm and polling report from netstorm.
   * @param TestRun
   * @param consoleLogger
   */
  private void connectNSAndPollJsonReport(Run build, PrintStream logg){
	   
	     try {

	     logger.log(Level.INFO, "Test is stopped. Now getting report from Netstorm. It may take some time. URL = " + pollReportURL);
	     logger.log(Level.INFO, "timeout =" + timeout);
	     
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
	        	  
	        	 // logger.log(Level.INFO, "Polling Response = " + pollResString);
	        	  JSONObject pollResponse = (JSONObject) JSONSerializer.toJSON(pollResString);
	        	      	  
	        	  /*Getting data, if not available.*/
	        	 // logger.log(Level.INFO, "Getting data as - "+pollResponse);
	                          	          	  
	        	  if(pollResponse.getBoolean("status")) {
	        	    /*Terminating Loop when test is stopped.*/
	        	    isReportGenerated = false;
	        	    String reportstatus = pollResponse.getString("Report Status");
	        	    logg.println("Report Status = " + reportstatus);
	        	    logger.log(Level.INFO, "report status = " + reportstatus);
	        	    
	        	    if(reportstatus.equals("PASS"))
	        	      build.setResult(Result.SUCCESS);
	        	    else
	        	      build.setResult(Result.FAILURE);
	                    /*Parsing to get the data from response. */
//	                      JSONParser parser = new JSONParser();
//	                      org.json.simple.JSONObject objJson = (org.json.simple.JSONObject) parser.parse(pollResString);
//	                      if(!objJson.isNull("data")) { 
//	                      String strData = objJson.get("data").toString();
//	                      
//	                       org.json.simple.JSONObject objJson2 =(org.json.simple.JSONObject)parser.parse(strData);
//	                       String strData2 = objJson2.toJSONString();
//	                       logger.log(Level.INFO, "Data -- = " + strData);
//	                       resonseReportObj = (JSONObject) JSONSerializer.toJSON(strData2);
//	                      }
	        	  }
	                  
	                  
	        	} catch (Exception e) {
	        	  logger.log(Level.SEVERE, "Error in parsing polling response = " + pollResString, e);
	        	  build.setResult(Result.UNSTABLE);
	        	  
	        	}

	        	/*Closing Stream.*/
	        	try {
	        	  br.close();
	        	} catch (Exception e) {
	        	  logger.log(Level.SEVERE, "Error in closing stream inside polling thread.", e);
	        	  build.setResult(Result.UNSTABLE);
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
	        	build.setResult(Result.UNSTABLE);
	              } 
	            }

	          } catch (Exception e) {
	            logger.log(Level.SEVERE, "Error in polling report with interval.", e);
	            build.setResult(Result.UNSTABLE);
	          }
	        }   
	      };

	      // Creating and Starting thread for Getting Graph Data.
	     // Thread pollTestRunThread = new Thread(pollReportState, "pollTestRunThread");

	      // Running it with Executor Service to provide concurrency.
	      ExecutorService threadExecutorService = Executors.newFixedThreadPool(1);

	      // Executing thread in thread Pool.
	      //threadExecutorService.execute(pollTestRunThread);
	      
	      final Future future = threadExecutorService.submit(pollReportState);
	      
	      // Shutting down in order.
	      threadExecutorService.shutdown();
	      
	      try { 
	    	  future.get(timeout, TimeUnit.MINUTES); 
	    	}
	    	catch (InterruptedException ie) { 
	    	  /* Handle the interruption. Or ignore it. */ 
	    	}
	    	catch (ExecutionException ee) { 
	    	  /* Handle the error. Or ignore it. */ 
	    	}
	    	catch (TimeoutException te) { 
	    	  /* Handle the timeout. Or ignore it. */ 
	    	}
	    	if (!threadExecutorService.isTerminated())
	    		threadExecutorService.shutdownNow();

	      // Checking for running state.
	      // Wait until thread is not terminated.
//	      while (!threadExecutorService.isTerminated())
//	      {
//	      }    
	       
	    } catch (Exception e) {
	      logger.log(Level.SEVERE, "Error in polling report.", e);
	    }
	   
	  }




  public boolean fetchMetricData(NetStormConnectionManager connection,  String metrics[], String durationInMinutes, int groupIds[], int graphIds[], int testRun, String testMode, PrintStream logg, String test_cycle_Number, final Run build)
  {
    logger.log(Level.INFO, "fetchMetricData() called.");
  
    JSONObject jsonRequest = makeRequestObject("GET_DATA");
    
    logger.log(Level.INFO, "json request----->",jsonRequest);
    jsonRequest.put("TESTRUN", String.valueOf(testRun));
    jsonRequest.put(JSONKeys.TESTMODE.getValue(), testMode);
    jsonRequest.put(JSONKeys.PROJECT.getValue(), connection.getProject());
    jsonRequest.put(JSONKeys.SUBPROJECT.getValue(), connection.getSubProject());
    jsonRequest.put(JSONKeys.SCENARIO.getValue(), connection.getScenario());
    jsonRequest.put("TestCycleNumber", test_cycle_Number);
    jsonRequest.put("isDurationPhase", durationPhase);
    jsonRequest.put("workProfile", connection.getProfile());
   
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
       
	    logger.log(Level.INFO, "jsonRequest " + jsonRequest);
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
                 //return null;
                 return false;
               }
            }
        
            pollReportURL = str+"/ProductUI/productSummary/jenkinsService/checkNetstormReportStatus";
        
            logger.log(Level.INFO, "url for polling report - pollReportURL = " + pollReportURL);
            
            connectNSAndPollJsonReport(build, logg);
        
//            if(resonseReportObj == null)
//            {
//               logger.log(Level.SEVERE, "Not able to get response form server due to: " + errMsg);
//               return null;
//            }
         }
         catch (MalformedURLException e) {
	   logger.log(Level.SEVERE, "Unknown exception in establishing connection. MalformedURLException -", e);
	   e.printStackTrace();
         } catch (IOException e) {
	   logger.log(Level.SEVERE, "Unknown exception in establishing connection. IOException -", e);
	   e.printStackTrace();
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

   // return parseJSONData(resonseReportObj, testMode, logg);
      return true;
  }  

  public String numberFormatWithDecimal(double value, String upToDecimal) {
	    try {
	      DecimalFormat df = new DecimalFormat(upToDecimal);
	      if(value%1 == 0){
	    	  upToDecimal = "0";
	      }
	      String num = df.format(value);
	      String arr[] = num.split("\\.");

	      arr[0] = NumberFormat.getIntegerInstance().format(Long.parseLong(arr[0]));
//	      arr[0] = NumberFormat.getIntegerInstance().format(arr[0]);
	      if(arr.length == 2) {
	        return arr[0] + "." + arr[1];
	      } else
	        return arr[0];

	      
	    } catch (Exception e) {
	    	logger.log(Level.SEVERE, "Unknown exception ", e);
	    }
	    return "";
	  }

  private MetricDataContainer parseJSONData(JSONObject resonseObj, String testMode, PrintStream logg)
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
        
        logg.println("----------------------------");
        logg.println("Overall Status = " + overAllStatus);
        
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
       if(jsonTestReport.get("Metrics Under Test") != null && jsonTestReport.get("Page Detail Report") == null) {
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

          String count = jsonObject.getString("Count");
          
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
          
          if(count.equals("noCount")){
        	  testMetric.setCount("NA");
          }
          else {
        	  testMetric.setCount(count);  
          }
          
          if(trendLink != null)
           testMetric.setLinkForTrend(trendLink);
          else
            testMetric.setLinkForTrend("NA");
            
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
          testMetric.setMetricLink(metricLink);
          testMetric.setMetricRuleName(metricRule);
          testMetric.setTransactiontStatus(transactiontStatus);
          testMetric.setStatusColour(transactionBgcolor);
          testMetric.setTransactionTooltip(transactionTooltip);
          testMetricsList.add(testMetric);
          testReport.setOperator(operator);
          testReport.setTestMetrics(testMetricsList);
          
          if(count.equals("noCount")){
        	  testReport.setShowCount("0");
          }else {
        	  testReport.setShowCount("1");
          }
          
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
       
       /*Page Detail Report*/
       if(jsonTestReport.get("Page Detail Report") != null) {
    	   JSONArray pageDetailReport = (JSONArray)jsonTestReport.get("Page Detail Report");
           ArrayList<PageDetail> pageDetailReportList = new ArrayList<PageDetail>(pageDetailReport.size());
           
           for(Object jsonData : pageDetailReport)
           {  
             JSONObject jsonObject = (JSONObject)jsonData;
             
             String maxPageLoad = jsonObject.getString("maxPageLoad");
             String strAvgPerfScore = jsonObject.getString("strAvgPerfScore");
             String strAvgBestPracScore = jsonObject.getString("strAvgBestPracScore");
             String minStartRender = jsonObject.getString("minStartRender");
             String strPageName = jsonObject.getString("strPageName");
             String strAverageByteRec = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("strAverageByteRec")), "0");
             
             String maxEndRender = jsonObject.getString("maxEndRender");
             String strBrowserAverageReq = jsonObject.getString("strBrowserAverageReq");
             String minOnload = jsonObject.getString("minOnload");
             String strAvgFrstCpuIdle = jsonObject.getString("strAvgFrstCpuIdle");
             String strHostName = jsonObject.getString("strHostName");
             String minEndRender = jsonObject.getString("minEndRender");
             String strBrowserAverageReqOnLoad = jsonObject.getString("strBrowserAverageReqOnLoad");
             String strAverageReqDomContent = jsonObject.getString("strAverageReqDomContent");
             String averageEndRender = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("averageEndRender")), "0.00");
             
             String strAverageDOM = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("strAverageDOM")), "0.00");
             
             String strAvgAccessScore = jsonObject.getString("strAvgAccessScore");
             String strAveragePageLoad = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("strAveragePageLoad")), "0.00");
             
             String averageSpeedIndex = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("averageSpeedIndex")), "0");
             String browserName = jsonObject.getString("browserName");
             String strAvgFrstCPaint = jsonObject.getString("strAvgFrstCPaint");
             String strAvgSeoScore = jsonObject.getString("strAvgSeoScore");
             String strAvgIptLtncy = jsonObject.getString("strAvgIptLtncy");
             String strAverageReqOnLoad = jsonObject.getString("strAverageReqOnLoad");
             String strAverageOnload = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("strAverageOnload")), "0.00");
             
             String strAvgPwaScrore = jsonObject.getString("strAvgPwaScrore");
             String minDom = jsonObject.getString("minDom");
             String maxStartRender = jsonObject.getString("maxStartRender");
             String strSessionCount = jsonObject.getString("strSessionCount");
             String averageStartRender = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("averageStartRender")), "0.00");
             
             String maxDom = jsonObject.getString("maxDom");
             String groupName = jsonObject.getString("groupName");
             String strAverageReq = jsonObject.getString("strAverageReq");
             String pageIndex = jsonObject.getString("pageIndex");
             String strBrowserAverageReqDomContent = jsonObject.getString("strBrowserAverageReqDomContent");
             String envName = jsonObject.getString("envName");
             String sessionPagename = jsonObject.getString("sessionPagename");
             String maxOnload = jsonObject.getString("maxOnload");
             String strAvgFrstMpaint = jsonObject.getString("strAvgFrstMpaint");
             String strAverageByteSent = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("strAverageByteSent")), "0");
             
             String strScreenSize = jsonObject.getString("strScreenSize");
             String strAverageTimeToInteract = numberFormatWithDecimal(Double.parseDouble(jsonObject.getString("strAverageTimeToInteract")), "0.00");
             
             String minPageLoad = jsonObject.getString("minPageLoad");
             
             
             PageDetail pageDetail = new PageDetail();
             pageDetail.setAverageEndRender(averageEndRender);
             pageDetail.setAverageSpeedIndex(averageSpeedIndex);
             pageDetail.setAverageStartRender(averageStartRender);
             pageDetail.setBrowserName(browserName);
             pageDetail.setEnvName(envName);
             pageDetail.setGroupName(groupName);
             pageDetail.setMaxDom(maxDom);
             pageDetail.setMaxEndRender(maxEndRender);
             pageDetail.setMaxOnload(maxOnload);
             pageDetail.setMaxPageLoad(maxPageLoad);
             pageDetail.setMaxStartRender(maxStartRender);
             pageDetail.setMinDom(minDom);
             pageDetail.setMinEndRender(minEndRender);
             pageDetail.setMinOnload(minOnload);
             pageDetail.setMinPageLoad(minPageLoad);
             pageDetail.setMinStartRender(minStartRender);
             pageDetail.setPageIndex(pageIndex);
             pageDetail.setSessionPagename(sessionPagename);
             pageDetail.setStrAverageByteRec(strAverageByteRec);
             pageDetail.setStrAverageByteSent(strAverageByteSent);
             pageDetail.setStrAverageDOM(strAverageDOM);
             pageDetail.setStrAverageOnload(strAverageOnload);
             pageDetail.setStrAveragePageLoad(strAveragePageLoad);
             pageDetail.setStrAverageReq(strAverageReq);
             pageDetail.setStrAverageReqDomContent(strAverageReqDomContent);
             pageDetail.setStrAverageReqOnLoad(strAverageReqOnLoad);
             pageDetail.setStrAverageTimeToInteract(strAverageTimeToInteract);
             pageDetail.setStrAvgAccessScore(strAvgAccessScore);
             pageDetail.setStrAvgBestPracScore(strAvgBestPracScore);
             pageDetail.setStrAvgFrstCPaint(strAvgFrstCPaint);
             pageDetail.setStrAvgFrstCpuIdle(strAvgFrstCpuIdle);
             pageDetail.setStrAvgFrstMpaint(strAvgFrstMpaint);
             pageDetail.setStrAvgIptLtncy(strAvgIptLtncy);
             pageDetail.setStrAvgPerfScore(strAvgPerfScore);
             pageDetail.setStrAvgPwaScrore(strAvgPwaScrore);
             pageDetail.setStrAvgSeoScore(strAvgSeoScore);
             pageDetail.setStrBrowserAverageReq(strBrowserAverageReq);
             pageDetail.setStrBrowserAverageReqDomContent(strBrowserAverageReqDomContent);
             pageDetail.setStrBrowserAverageReqOnLoad(strBrowserAverageReqOnLoad);
             pageDetail.setStrHostName(strHostName);
             pageDetail.setStrPageName(strPageName);
             pageDetail.setStrScreenSize(strScreenSize);
             pageDetail.setStrSessionCount(strSessionCount);
             
             pageDetailReportList.add(pageDetail);
             
           }
           
           JSONObject pageDetailObj = (JSONObject)pageDetailReport.get(0);
           String strScreenSize = pageDetailObj.getString("strScreenSize");
           String strAverageTimeToInteract = pageDetailObj.getString("strAverageTimeToInteract");
           
           ArrayList<String> pageDetailHeader = new ArrayList<String>();
           pageDetailHeader.add("Page Name");
           pageDetailHeader.add("Host Name");
           pageDetailHeader.add("Group");
           pageDetailHeader.add("Browser");
//           if(!strScreenSize.equals(""))
           pageDetailHeader.add("Screen Size");
           pageDetailHeader.add("Session Count");
           pageDetailHeader.add("DOM Content Load(Sec)");
           pageDetailHeader.add("On Load(Sec)");
           pageDetailHeader.add("Page Load(Sec)");
//           if(!strAverageTimeToInteract.equals("-1"))
           pageDetailHeader.add("Time To Interact(Sec)");
           pageDetailHeader.add("Start Render Time(Sec)");
           pageDetailHeader.add("Visually Complete Time");
           pageDetailHeader.add("Requests");
           pageDetailHeader.add("Browser Cache");
           pageDetailHeader.add("Bytes Received(KB)");
           pageDetailHeader.add("Bytes Send(KB)");
           pageDetailHeader.add("Speed Index");
           
           
           testReport.setPageDetailHeader(pageDetailHeader);
           testReport.setPageDetail(pageDetailReportList);
           
           logger.log(Level.INFO,"Page Detail report : " + testReport.getPageDetail());
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
        
        if(!testRun.equals("-1")) {
        	String url = getUrlString();
        	String reportlink = url + "/logs/TR" + testRun + "/ready_reports/TestSuiteReport.html";
        	logg.println("Report Link:- " + reportlink);
        }
        
        logg.println("----------------------------");
        
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
								  String linkss = (String)metricInfoFinal.get("Trend Link");
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
								 // finalInfoForVector.setLink(linkss);
								  finalInfoForVector.setTrendLink(linkss);
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
							  String links = (String)metricObj.get("Trend Link");
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
							 // transValue.setLink(links);
							  transValue.setTrendLink(links);
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
				  
				  String trendLink = (String)scalarVal.get("Trend Link");
				  
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
 
  public void updateNCDataFile(FilePath fp, String scriptName, String username, String profile, PrintStream consoleLogger) {
	  try {
		  URL url;
		  String str =   getUrlString();//URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));


		  url = new URL(str+"/ProductUI/productSummary/jenkinsService/updateDataFiles?scriptName=" + scriptName + "&username="  + username + "&profile=" + profile);

		  logger.log(Level.INFO, "url for updating data files = "+  url);
		  String Boundary = UUID.randomUUID().toString();
		  HttpURLConnection connect = (HttpURLConnection) url.openConnection();
		  connect.setConnectTimeout(0);
		  connect.setReadTimeout(0);
		  connect.setRequestMethod("POST");
		  connect.setRequestProperty("Content-Type", "multipart/form-data; boundary="+Boundary);

		  connect.setDoOutput(true);

		  DataOutputStream out = new DataOutputStream(connect.getOutputStream());
		  out.writeUTF("--"+Boundary+"\r\n"
				  +"Content-Disposition: form-data; name=\"file\"; filename="+ fp.getName() + "\r\n"
				  +"Content-Type: application/octet-stream; charset=utf-8"+"\r\n\r\n");
		  InputStream in = fp.read();
		  byte[] b = new byte[1024];
		  int l = 0;
		  while((l = in.read(b)) != -1) out.write(b,0,l); // Write to file
		  out.writeUTF("\r\n--"+Boundary+"--\r\n");
		  out.flush();
		  out.close();

		  logger.log(Level.INFO, "sttaus code = " + connect.getResponseCode());
		  if (connect.getResponseCode() == 200) {
			  BufferedReader br = new BufferedReader(new InputStreamReader((connect.getInputStream())));
			  JSONObject output = (JSONObject) JSONSerializer.toJSON(br.readLine());
			  String mssg = (String) output.get("Message");
			  logger.log(Level.INFO, "mssg = " + mssg);
			  consoleLogger.println(mssg);

		  } else {
			  consoleLogger.println("Error in uploading data files.");
		  }
		  
		  if(fp.exists()) {
			  fp.delete();
			logger.log(Level.INFO, "Uploaded file deleted successfully.");
		  }
		  
	  } catch(Exception e) {
         consoleLogger.println("Error in uploading data files.");
            }
  }
 
  public JSONArray getScriptList(String profile,String scenario,String project,String subProject,String testMode){
	  try{
	  	 JSONArray scripts = new JSONArray();
		 URL url ;
      String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
	      url = new URL(str+"/ProductUI/productSummary/ScenarioWebService/getScenarioScriptList?scenName="+scenario+"&userName="+username+"&profile="+profile+"&project="+project+"&subproject="+subProject+"&testMode="+testMode);
	     
	      logger.log(Level.INFO, "getScriptList url-"+  url);
	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
	      conn.setRequestProperty("Content-Type","application/json");
	      if(conn.getResponseCode()!= 200) {
	    	  logger.log(Level.INFO, "getting error in fetching script list.");
	    	  scripts = new JSONArray();
	    	  return scripts;
	      }
	      
	      BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));
	      StringBuilder stb = new StringBuilder();
	      
	      String response = null;
	      while((response = br.readLine())!= null) {
	    	  stb.append(response);
	      }
	      logger.log(Level.INFO, "response of page list - " + stb.toString());
	      scripts = JSONArray.fromObject(stb.toString());
	      logger.log(Level.INFO, "scripts size - " + scripts.size());
	      return scripts;
	  }catch(Exception e){
		  logger.log(Level.SEVERE, "Exception in getting script list - " + e);
		  return null;
	  }
}
  
  public JSONArray getPageList(String script,String scenario,String profile,String testMode,String project,String subProjects){
	  try{
	  	 JSONArray scripts = new JSONArray();
		 URL url ;
         String str = getUrlString(); // URLConnectionString.substring(0,URLConnectionString.lastIndexOf("/"));
	      url = new URL(str+"/ProductUI/productSummary/ScenarioWebService/getPageListByScriptNameJenkins?scenMode=0&scenName="+scenario+"&userName="+username+"&scriptName="+script+"&profile="+profile+"&project="+project+"&subproject="+subProjects+"&testMode="+testMode);
	     
	      logger.log(Level.INFO, "getPageList url-"+  url);
	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
	      conn.setRequestProperty("Content-Type","application/json");
	      if(conn.getResponseCode()!= 200) {
	    	  logger.log(Level.INFO, "getting error in fetching page list.");
	    	  scripts = new JSONArray();
	    	  return scripts;
	      }
	      
	      BufferedReader br = new BufferedReader(new InputStreamReader(conn.getInputStream()));
	      StringBuilder stb = new StringBuilder();
	      
	      String response = null;
	      while((response = br.readLine())!= null) {
	    	  stb.append(response);
	      }
	      logger.log(Level.INFO, "response of page list - " + stb.toString());
	      scripts = JSONArray.fromObject(stb.toString());
	      logger.log(Level.INFO, "scripts size - " + scripts.size());
	      return scripts;
	  }catch(Exception e){
		  logger.log(Level.SEVERE, "Exception in getting page list - " + e);
		  return null;
	  }
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
