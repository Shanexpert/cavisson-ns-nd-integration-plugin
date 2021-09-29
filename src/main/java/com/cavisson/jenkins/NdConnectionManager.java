package com.cavisson.jenkins;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
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
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.zip.GZIPInputStream;
import java.io.PrintStream;

import net.sf.json.JSONArray;
import net.sf.json.*;
import javax.net.ssl.HostnameVerifier;
import javax.net.ssl.HttpsURLConnection;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSession;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;

import com.cavisson.jenkins.NetDiagnosticsParamtersForReport;

import hudson.util.Secret;



public class NdConnectionManager {

  private final String URLConnectionString;
  private URLConnection urlConn = null;
  private transient final Logger logger = Logger.getLogger(NdConnectionManager.class.getName());
  private String username = "";
  private Secret password;
  NetDiagnosticsParamtersForReport ndParam;
  private String restUrl = "";
  private String result;
  private boolean isNDE = false;
  private String curStart;
  private String curEnd;
  private JSONObject jkRule = new JSONObject();
  private String critical;
  private String warning;
  private String overall;
  private String err = "Connection failure, please check whether Connection URI is specified correctly";
  
   public String getCritical() {
    return critical;
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

  public String getResult() {
    return result;
  }

  public void setResult(String result) {
    this.result = result;
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

  public NdConnectionManager(String uRLConnectionString, String username, Secret password, boolean isNDE) {

    URLConnectionString = uRLConnectionString;
    this.username = username;
    this.restUrl = uRLConnectionString;
    this.isNDE = isNDE;
    this.password = password;
  }

  public NdConnectionManager(String uRLConnectionString, String username, Secret password, NetDiagnosticsParamtersForReport ndParam, boolean isNDE) {

    URLConnectionString = uRLConnectionString;
    this.username = username;
    this.ndParam = ndParam;
    this.restUrl = uRLConnectionString;
    this.isNDE = isNDE;
    this.password = password;
  }

  /**
   * This Method makes connection to netdiagnostics.
   *
   * @param errMsg
   * @return true , if Successfully connected and authenticated false ,
   * otherwise
   */
  public boolean testNDConnection(StringBuffer errMsg, String test) 
  {
    logger.log(Level.INFO, "testNDConnection() method is  called. rest url -"+ restUrl);
    if(checkAndMakeConnection(URLConnectionString, restUrl, errMsg, test))
    {
      logger.log(Level.INFO, "After check connection method.");

//      if((getResult() == null))
//      {
//	logger.log(Level.INFO, "result is null- ",err);
//	errMsg.append("Connection failure, please check whether Connection URI is specified correctly");
//	return false;
//      }
//      else 
//      {
//	logger.log(Level.INFO, "Successfully Authenticated.");   
//	return true;
//      }
     return true;
    } 
    else 
    {
      logger.log(Level.INFO, "connection is fail - ",err);
      errMsg.append(err);
      return false;
    }

  }

  private boolean checkAndMakeConnection(String urlString, String restUrl, StringBuffer errMsg)
  {
    return checkAndMakeConnection(urlString, restUrl,errMsg, null);
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
  private boolean checkAndMakeConnection(String urlString, String restUrl, StringBuffer errMsg, String test)
  {
    logger.log(Level.INFO, "checkAndMakeConnection method called. with arguments restUrl : ", new Object[]{restUrl});
    try
    {
       restUrl = getUrlString();
      if(ndParam != null)
      {
	logger.log(Level.INFO, "checkAndMakeConnection method called. with ndParam "+  ndParam.toString());
	ndParam.setCurStartTime(this.getCurStart().replace(" ", "@"));
	ndParam.setCurEndTime(this.getCurEnd().replace(" ", "@"));
	if(ndParam.getBase1StartTime() != null && !ndParam.getBase1StartTime().equals(""))
		ndParam.setBase1StartTime(ndParam.getBase1StartTime().replace(" ", "@"));
	else
	   ndParam.setBase1StartTime(null);  
		
		if(ndParam.getBase1EndTime() != null && !ndParam.getBase1EndTime().equals(""))
		  ndParam.setBase1EndTime(ndParam.getBase1EndTime().replace(" ", "@"));
		else
		  ndParam.setBase1EndTime(null);
		
		if(ndParam.getBase2StartTime() != null && !ndParam.getBase2StartTime().equals(""))
		  ndParam.setBase2StartTime(ndParam.getBase2StartTime().replace(" ", "@"));
		else
		  ndParam.setBase2StartTime(null);

		if(ndParam.getBase2EndTime() != null &&  !ndParam.getBase2EndTime().equals(""))
		  ndParam.setBase2EndTime(ndParam.getBase2EndTime().replace(" ", "@"));
		else
		  ndParam.setBase2EndTime(null);
		
		if(ndParam.getBase3StartTime() != null && !ndParam.getBase3StartTime().equals(""))
		  ndParam.setBase3StartTime(ndParam.getBase3StartTime().replace(" ", "@"));
	    else
		  ndParam.setBase3StartTime(null);

	    if(ndParam.getBase3EndTime() != null &&  !ndParam.getBase3EndTime().equals(""))
		  ndParam.setBase3EndTime(ndParam.getBase3EndTime().replace(" ", "@"));
	    else
		  ndParam.setBase3EndTime(null);
	    
	    if(ndParam.getBase1MSRName().equals(""))
		  ndParam.setBase1MSRName(null);
	    
	    if(ndParam.getBase2MSRName().equals(""))
			  ndParam.setBase2MSRName(null);
	    
	    if(ndParam.getBase3MSRName().equals(""))
			  ndParam.setBase3MSRName(null);
      }

      if(this.getCritical() != null && this.getCritical() != "")
        ndParam.setCritiThreshold(this.getCritical().replace(" ", "@"));

      if(this.getWarning() != null && this.getWarning() != "")
        ndParam.setWarThreshold(this.getWarning().replace(" ", "@"));

      if(this.getOverall() != null && this.getOverall() != "")
        ndParam.setFailThreshold(this.getOverall().replace(" ", "@"));

      URL url ;

      if(test == null || test.equals(null))
	url = new URL((restUrl+"/ProductUI/productSummary/jenkinsService/reportData?reportParam="+ndParam.toString()+"&status=false"+"&chkRule="+this.getJkRule()).replaceAll(" ","%20"));
      else
	url = new URL((restUrl+"/ProductUI/productSummary/jenkinsService/reportData?reportParam="+test+"&status=true").replaceAll(" ","%20")); //for only test connection

      logger.log(Level.INFO, "checkAndMakeConnection method called. with arguments url"+  url);
     
      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
      conn.setConnectTimeout(0);
	  conn.setReadTimeout(0);
      conn.setRequestMethod("GET");
      conn.setRequestProperty("Accept", "application/json");
      conn.setDoOutput(false);
      if (conn.getResponseCode() != 200) {
	throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
      }

      BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
      setResult(br.readLine());
      logger.log(Level.INFO, "RESPONSE -> "+getResult());
      
      JSONObject resonseObj = null;
      resonseObj =  (JSONObject) JSONSerializer.toJSON(this.result);
      System.out.println("resonseObj -- "+resonseObj);
      Boolean status = (Boolean)resonseObj.get("status");
         
//      if(status == false && !((String)resonseObj.get("errMsg")).equals(""))
//        err = (String)resonseObj.get("errMsg");
      
      return status;
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

  public MetricDataContainer fetchMetricData( String metrics[], String durationInMinutes, int groupIds[], int graphIds[], int testRun, String testMode)
  {
    logger.log(Level.INFO, "fetchMetricData() called. get resulet"+ getResult());
    JSONObject resonseObj = null;
    resonseObj =  (JSONObject) JSONSerializer.toJSON(this.result);
    if(resonseObj == null)
    {
      logger.log(Level.SEVERE, "Not able to get response form server due to: ");
      return null;
    }

    return parseJSONData(resonseObj, testMode);
  }

  private MetricDataContainer parseJSONData(JSONObject resonseObj, String testMode)
  {
    logger.log(Level.INFO, "parseJSONData() called.");

    MetricDataContainer metricDataContainer = new MetricDataContainer();
    logger.log(Level.INFO,"Metric Data:" + metricDataContainer );
    logger.log(Level.INFO,"Recived response from : " + resonseObj );
    System.out.println("Recived response from : " + resonseObj);

    try{
      ArrayList<MetricData> dataList = new ArrayList<MetricData>();
      JSONObject jsonGraphs = (JSONObject)resonseObj.get("graphs");
      logger.log(Level.INFO,"Recived response from graphs : " + jsonGraphs );
      int freq = ((Integer)resonseObj.get("frequency"))/1000; 
      logger.log(Level.INFO,"Recived response from : frq = " + freq );
      metricDataContainer.setFrequency(freq);

      if(resonseObj.containsKey("customHTMLReport"))
	metricDataContainer.setCustomHTMLReport((String)resonseObj.get("customHTMLReport"));

      String prefix[] ;
      prefix = URLConnectionString.split(("//"));
      
      TestReport testReport = new TestReport();
      if("T".equals(testMode))
      {
	logger.log(Level.INFO,"Recived response inside test mode : "  );
	testReport = new TestReport();
	JSONObject jsonTestReportWholeObj = resonseObj.getJSONObject("testReport");
	logger.log(Level.INFO,"Recived response from whole report : " + jsonTestReportWholeObj );
	JSONObject jsonTestReport = jsonTestReportWholeObj.getJSONObject("members");

	logger.log(Level.INFO,"Recived response from test report : " + jsonTestReport );
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
        String dashboardURL = jsonTestReport.getString("Dashboard Link");
        String reportLink = jsonTestReport.getString("Report Link");
	String currentDateTime = "", previousDescription = "", baselineDescription = "", currentDescription = "", initialDescription = "";
	try{
	  currentDateTime = jsonTestReport.getString("Current Date Time");
	  previousDescription = jsonTestReport.getString("Previous Description"); 
	  baselineDescription = jsonTestReport.getString("Baseline Description");
	  currentDescription =  jsonTestReport.getString("Current Description");
	  initialDescription = jsonTestReport.getString("Initial Description");
	}
	catch(Exception ex) {
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
	  String currValue = jsonObject.getString("Value");
	  String edLink = jsonObject.getString("link");
	  String metric = jsonObject.getString("Metric");
	  String metricRule = jsonObject.getString("MetricRule");
	  String operator = jsonObject.getString("Operator");
	  String sla = jsonObject.getString("SLA");
	  
	  String count = jsonObject.getString("Count");
	  
	  if(sla.indexOf(">") != -1 || sla.indexOf(">") > 0)
	    sla = sla.substring(sla.lastIndexOf(">")+1, sla.length());

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
	  
//	  if (trendLink != null) {
//             if(prefix[0] != "")
//              trendLink = prefix[0] + trendLink;
             
            testMetric.setLinkForTrend(trendLink);
//          } else {
//            testMetric.setLinkForTrend("NA");
//          }
          
	  testMetric.setMetricName(metric);
          
//          if(prefix[0] != "")
//           metricLink = prefix[0] + metricLink;
	  
	  int fromPattern=0;
	  String patt = jsonObject.getString("PATTERN");
	  //      String arrMetricDisplayName = arrMetricValue[0];
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

	if(!isNDE)
	{
	  for(int i = 0 ; i < transObj; i++)
	  {
	    JSONObject transactionJson = null;

	    if(i == 1)
	      transactionJson = jsonTestReport.getJSONObject("BASETOT");
	    else 
	    {  
	      if(jsonTestReport.has("TOT"))
		transactionJson = jsonTestReport.getJSONObject("TOT");
	      else
		transactionJson = jsonTestReport.getJSONObject("CTOT"); 
	    }

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
	      transactionStats.setTransTestRun("BASETOT");
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
	}
	else
	  testReport.setShowHideTransaction(true); 


	if(isNDE)
	{
//	if(ndParam.getBaseStartTime() != null && !ndParam.getBaseStartTime().equals("") && !ndParam.getBaseStartTime().equals("NA")) {		
//	  String[] startDateTime = ndParam.getBaseStartTime().split("@");         
//	  String[] endDateTime = ndParam.getBaseEndTime().split("@");
//
//	  System.out.println("start time == "+startDateTime[0]);
//	  System.out.println("end 9time == "+endDateTime[0]);
//	  if(startDateTime[0].equals(endDateTime[0]))
//	    baseLineDateTime = ndParam.getBaseStartTime().replace("@", " ") + " to "+ endDateTime[1];
//	  //baseLineDateTime = startDateTime[0] + " to "+ endDateTime[1];
//	  else
//	    baseLineDateTime = ndParam.getBaseStartTime().replace("@", " ") + " to "+ ndParam.getBaseEndTime().replace("@", " ");
//	}

	String[] curStartDateTime = ndParam.getCurStartTime().split("@");
	  String[] curEndDateTime = ndParam.getCurEndTime().split("@");
	  if(curStartDateTime[0].equals(curEndDateTime[0]))
	    currentDateTime = ndParam.getCurStartTime().replace("@", " ") + " to "+ curEndDateTime[1];
	  else
	    currentDateTime =  ndParam.getCurStartTime().replace("@", " ") + " to "+ ndParam.getCurEndTime().replace("@", " ");

//	  if(ndParam.getInitEndTime() != null)
//	  {
//	    String[] initStartDateTime = ndParam.getInitStartTime().split("@");	
//	    String[] initEndDateTime = ndParam.getInitEndTime().split("@");
//	    if(initStartDateTime[0].equals(initEndDateTime[0]))
//	      initialDateTime = ndParam.getInitStartTime().replace("@", " ") + " to "+ initEndDateTime[1];
//	    else
//	      initialDateTime = ndParam.getInitStartTime().replace("@", " ") + " to "+ ndParam.getInitEndTime().replace("@", " ");
//	  } 
          
          if(ndParam.isPrevDuration()){
            
           String  prevStartTime = ndParam.getCurStartTime().replace("@"," ");
           String prevEndTime = ndParam.getCurEndTime().replace("@"," ");
           
           SimpleDateFormat trDateFormat = new SimpleDateFormat("MM/dd/yy HH:mm:ss");
           
           Date userStartDate = trDateFormat.parse(prevStartTime); 
           long startTime = userStartDate.getTime();
           
            Date userEndDate = trDateFormat.parse(prevEndTime); 
           long endTime = userEndDate.getTime();
           
           long diff = (endTime- startTime);
           long prevStartTS = startTime - diff;
           long prevEndTS = startTime;
           
           Date startDatePrev = new Date(prevStartTS);
           String prevStartDuration = trDateFormat.format(startDatePrev);
           
           Date endDatePrev = new Date(prevEndTS);
           String prevEndDuration = trDateFormat.format(endDatePrev);
           
           
           String[] prevStartDateTime = prevStartDuration.split(" ");
	       String[] prevEndDateTime = prevEndDuration.split(" ");
              
           System.out.println("prevStartDuration = "+prevStartDuration +" , prevEndDuration = "+prevEndDuration);
            if(prevStartDateTime[0].equals(prevEndDateTime[0]))
              previousDateTime = prevStartDuration + " to "+ prevEndDateTime[1]; 
            else
              previousDateTime = prevStartDuration + " to "+ prevEndDuration;
            
          }
          

	}
//         if(prefix[0] != "")
//          dashboardURL = prefix[0]+dashboardURL;
         
        testReport.setDashboardURL(dashboardURL);
        
//         if(prefix[0] != "")
//          reportLink = prefix[0]+reportLink;
         
        testReport.setReportLink(reportLink);
	testReport.setBaseLineTestRun(baseLineTestRun);
	testReport.setInitialTestRun(initialTestRun);
	testReport.setBaselineDateTime(baseLineDateTime);
	testReport.setPreviousDateTime(previousDateTime);
	testReport.setInitialDateTime(initialDateTime);
	testReport.setOverAllFailCriteria(overAllFailCriteria);
	testReport.setDate(date);
	//testReport.setTestMetrics(testMetricsList);
	testReport.setOverAllStatus(overAllStatus);
	testReport.setServerName(serverName);
	testReport.setIpPortLabel(productName);
	testReport.setPreviousTestRun(previousTestRun);
	testReport.setTestRun(testRun);
	testReport.setNormalThreshold(normalThreshold);
	testReport.setCriticalThreshold(criticalThreshold);
	testReport.setCurrentDateTime(currentDateTime);
	testReport.setPreviousDescription(previousDescription);
	testReport.setBaselineDescription(baselineDescription);
	testReport.setInitialDescription(initialDescription);
	testReport.setCurrentDescription(currentDescription);
	metricDataContainer.setTestReport(testReport);
      }

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
        }else
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

}
