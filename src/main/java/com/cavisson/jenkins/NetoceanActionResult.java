package com.cavisson.jenkins;

import java.util.logging.Level;
import java.util.logging.Logger;
import java.io.File;
import java.io.*;
import java.io.FileWriter;
import java.io.PrintStream;
import net.sf.json.JSONArray;
import net.sf.json.*;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLConnection;
import javax.net.ssl.HttpsURLConnection;
import hudson.util.Secret;
import org.apache.commons.lang.StringUtils;

public class NetoceanActionResult {
    
     private String URLConnectionString;
     private String username;
     private final Secret password;
     private transient static final Logger logger = Logger.getLogger(NetoceanActionResult.class.getName());
     private String result;
     private JSONObject resonseReportObj = null;
	
	 public NetoceanActionResult(String URLConnectionString, String username, String password) {
               logger.log(Level.INFO, "inside a netocean constructor..............");
               this.URLConnectionString = URLConnectionString;
               this.username = username;
               this.password = StringUtils.isEmpty(password) ? null : Secret.fromString(password);
	 }
         
         public String getResult() {
                logger.log(Level.INFO, "getting a result..............");
                return "getting a result from netocean";
         }
         
         public Secret getPassword() {
         	return password;
         }
         
         /*activating a HPD Server*/
         public String activateHPD() {
             try {
                NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
                
              URL url;
    	      url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/restartHPD");
    	     
    	      logger.log(Level.INFO, "startNetstormTest. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
              
               if (conn.getResponseCode() != 200) {
	       logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
	       throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
	      }
               
              
               BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
               this.result = br.readLine();
               logger.log(Level.INFO, "RESPONSE -> "+getResult());
      
             JSONObject resonseObj = null;
             resonseObj =  (JSONObject) JSONSerializer.toJSON(this.result);
             System.out.println("resonseObj -- "+resonseObj);
             String status = (String)resonseObj.get("status"); 
             
             String result = status + "\n" + (String)resonseObj.get("hpdStatus");
             
             logger.log(Level.INFO, "status for activating a hpd -> " + result);
             
              return result;
             } catch (Exception e) {
            	 logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
                  return "Error in activating a netocean server";
             }
         }
         
         
     /*Creating a service in netocean server*/
     public String createService(String service, String urls, String template, String reqData, String resdata,
             String user) {
         try {
             
            /*creating a connection with m/c*/ 
            NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
            
            JSONObject jsonResponse  = null;
            /*Json object for creating a service*/
             JSONObject jsonRequest1 = new JSONObject();
             jsonRequest1.put("service", service);
             jsonRequest1.put("url", urls);
             jsonRequest1.put("template", template);
             jsonRequest1.put("respData", resdata);
             jsonRequest1.put("reqData", reqData);
             jsonRequest1.put("user", user);
             jsonRequest1.put("host", "default");
             jsonRequest1.put("contentType", "");
             jsonRequest1.put("activeStatus", "yes");
             jsonRequest1.put("duplicate", "no");
             jsonRequest1.put("regexp", "");
             jsonRequest1.put("isRtc", "true");
             jsonRequest1.put("isWsdl", "false");
             
             JSONObject jsonRequest = jsonRequest1;
             
             logger.log(Level.INFO, "json request----- for creation of service>",jsonRequest);
             
             /*Url object for intialize a req*/
             URL url;
	     url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/addNetoceanService");
	     
	    logger.log(Level.INFO, "creating a service url for service creation. "+  url);
	    HttpURLConnection conn = (HttpURLConnection) url.openConnection();
          
	    conn.setRequestMethod("POST");
        
	    conn.setRequestProperty("Accept", "application/json");
        
	    String json = jsonRequest.toString();
	    conn.setDoOutput(true);
	    OutputStream os = conn.getOutputStream();
	    os.write(json.getBytes());
	    os.flush();

	    if (conn.getResponseCode() != 200) {
	      throw new RuntimeException("Failed in creation of service : HTTP error code : "+ conn.getResponseCode());
	    }

            BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
            resonseReportObj =  (JSONObject) JSONSerializer.toJSON(br.readLine());
	    logger.log(Level.INFO, "Response for getting Json report   -> "+resonseReportObj);
            
            
              
            jsonResponse  =  (JSONObject) JSONSerializer.toJSON(resonseReportObj);
            
            logger.log(Level.INFO, "Response for getting Json report   -> "+jsonResponse);
            
            Boolean status = (Boolean)jsonResponse.get("status"); 
             
             if (status == true) {
                    return "Service created successfully.";
             } else {
                 return "Error in creating a service.";
             }
            
            
         } catch (Exception e) {
        	 logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
             return "Error in creating a Service.";
         }
     }
     
     
     
     /*Enable a service*/
     public String enableService(String service, String enableUrl, String user) {
         try {
                
            /*creating a connection with m/c*/ 
            NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
            
            String enableUrlinfo = "?services=" + service + "&enableUrl=" + enableUrl + "&user=" + user;
              /*Url object for intialize a req*/
            URL url;
	    url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/enableServices"+enableUrlinfo);
	     
              logger.log(Level.INFO, "enableUrl. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
              
               if (conn.getResponseCode() != 200) {
	       logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
	       throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
	      }
               
              
               BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
               this.result = br.readLine();
               logger.log(Level.INFO, "RESPONSE -> "+getResult());
      
             JSONObject resonseObj = null;
             resonseObj =  (JSONObject) JSONSerializer.toJSON(this.result);
             System.out.println("resonseObj getting response-- "+resonseObj);
             String status = (String)resonseObj.get("status"); 
                           
             logger.log(Level.INFO, "status for enabling a service-> " + status);
             
             return status;
             
         } catch (Exception e) {
        	 logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
             return "Error in enable a service.";
         }
     }
     
     
     
      /*Disable a service*/
     public String disableService(String service, String disableUrl, String user) {
         try {
                /*creating a connection with m/c*/ 
            NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
            
            String disableUrlInfo = "?services=" + service + "&disableUrl=" + disableUrl + "&user=" + user;
              /*Url object for intialize a req*/
            URL url;
	    url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/disableServices"+disableUrlInfo);
	     
              logger.log(Level.INFO, "disableUrl. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
              
               if (conn.getResponseCode() != 200) {
	       logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
	       throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
	      }
               
               
               BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
               
               String result = br.readLine();
               
               logger.log(Level.INFO, "RESPONSE -> "+ result);
      
             JSONObject resonseObj = null;
             resonseObj =  (JSONObject) JSONSerializer.toJSON(result);
             System.out.println("resonseObj getting response-- "+resonseObj);
             String status = (String)resonseObj.get("status"); 
                           
             logger.log(Level.INFO, "status for disabling a service-> " + status);
             
             return "service disable successfully.";
         
         } catch (Exception e) {
        	 logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
             return "Error in disable a service.";
         }
     }
     
     
     /*Deleting a service*/
      public String deleteService(String service) {
         try {
                  /*creating a connection with m/c*/ 
            NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
            
            String disableUrlInfo = "?host=default&deleteServices=" + service; 
            
            /*Url object for intialize a req*/
            URL url;
	    url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/deleteService"+disableUrlInfo);
	     
             logger.log(Level.INFO, "disableUrl. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
              
               if (conn.getResponseCode() != 200) {
	       logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
	       throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
	      }
               
              
               BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
               this.result = br.readLine();
               logger.log(Level.INFO, "RESPONSE -> "+getResult());
      
             JSONObject resonseObj = null;
             resonseObj =  (JSONObject) JSONSerializer.toJSON(this.result);
             System.out.println("resonseObj getting response-- "+resonseObj);
             String status = (String)resonseObj.get("status"); 
                           
             logger.log(Level.INFO, "status for deleting a service-> " + status);
             
             
             return status;
         } catch (Exception e) {
        	 logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
             return "Error in delete a service.";
         }
     }
      
      
      
    /*Rest api call for importing a service*/
      public String exportNetoceanService(String tarName, String services) {
          try {
            /*creating a connection with m/c*/ 
            NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
            
            String exportUrlInfo = "?tarName=" + tarName + "&serviceName=" + services; 
            
            /*Url object for intialize a req*/
            URL url;
	    url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/exportService"+exportUrlInfo);
	     
             logger.log(Level.INFO, "export service. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
              
               if (conn.getResponseCode() != 200) {
	       logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
	       throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
	      }
               
              
               BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
               this.result = br.readLine();
               logger.log(Level.INFO, "RESPONSE -> "+getResult());
      
             JSONObject resonseObj = null;
             resonseObj =  (JSONObject) JSONSerializer.toJSON(this.result);
             System.out.println("resonseObj getting response-- "+resonseObj);
             String status = (String)resonseObj.get("msg"); 
             logger.log(Level.INFO, "status for deleting a service-> " + status);
             
             return status;
          } catch (Exception e) {
        	  logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
             return "Error in importing a service.";
          }
      }

          /*Rest api call for importing a service*/
      public String copyExportJar(String hostIp, String filepath, String directory, String isExport) {
          try {
            /*creating a connection with m/c*/ 
            NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
             String filePath = "";
            if (isExport.equals("true")) {
             filePath = "$NS_WDIR/webapps/" + filepath;
            } else {
             filePath = "/tmp/" + filepath;   
            }
             JSONObject jsonResponse  = null;
            /*Json object for creating a service*/
             JSONObject jsonRequest = new JSONObject();
             jsonRequest.put("IP", hostIp);
             jsonRequest.put("path", filePath);
             jsonRequest.put("dir", directory);
             
             /*Url object for intialize a req*/
             URL url;
	     url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/nsuServerAdmin");
	     
	    logger.log(Level.INFO, "creating a service url for service creation. "+  url);
	    HttpURLConnection conn = (HttpURLConnection) url.openConnection();
          
	    conn.setRequestMethod("POST");
        
	    conn.setRequestProperty("Accept", "application/json");
        
	    String json = jsonRequest.toString();
	    conn.setDoOutput(true);
	    OutputStream os = conn.getOutputStream();
	    os.write(json.getBytes());
	    os.flush();

	    if (conn.getResponseCode() != 200) {
	      throw new RuntimeException("Failed in Exporting a service : HTTP error code : "+ conn.getResponseCode());
	    }

            BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
            resonseReportObj =  (JSONObject) JSONSerializer.toJSON(br.readLine());
	    logger.log(Level.INFO, "Response for getting Json service   -> "+resonseReportObj);
            
            
              
            jsonResponse  =  (JSONObject) JSONSerializer.toJSON(resonseReportObj);
            
            logger.log(Level.INFO, "Response for getting Json report   -> "+jsonResponse);
            
            String status = (String)jsonResponse.get("status"); 
             
           return status;
            
          } catch (Exception e) {
        	  logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
             return "Error in exporting a service tar.";
          }
      }
      
  
   /*Method for importing a services*/
    public String importServices(String tarName) {
        try {
             /*creating a connection with m/c*/ 
            NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
            
            String importurlInfo = "?tarName=" + tarName;
              /*Url object for intialize a req*/
            URL url;
	    url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/importFileForJenkins"+importurlInfo);
	     
              logger.log(Level.INFO, "importUrl. method called. with arguments for metric  url"+  url);
    	      HttpURLConnection conn = (HttpURLConnection) url.openConnection();
	      conn.setRequestMethod("GET");
	      conn.setRequestProperty("Accept", "application/json");
              
               if (conn.getResponseCode() != 200) {
	       logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
	       throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
	      }
               
              
             BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
             this.result = br.readLine();
             logger.log(Level.INFO, "RESPONSE -> "+getResult());
      
             JSONObject resonseObj = null;
             resonseObj =  (JSONObject) JSONSerializer.toJSON(this.result);
             System.out.println("resonseObj getting response-- "+resonseObj);
             String status = (String)resonseObj.get("status"); 
             logger.log(Level.INFO, "status for importing a service-> " + status);
             
            return status;
        } catch (Exception e) {
        	logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
             return "Error in importing a services.";
          }
    }
    
    
   /*Method for apply Rtc for service deployment*/ 
    public String applyRtc(String cmdArgs, String isTemp, String level, String service) {
        try {
            /*creating a connection with m/c*/ 
            NetStormConnectionManager netStormConnectionManager = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
             
             JSONObject jsonResponse  = null;
            /*Json object for creating a service*/
             JSONObject jsonRequest = new JSONObject();
             jsonRequest.put("cmdArgs", cmdArgs);
             jsonRequest.put("isTemp", isTemp);
             jsonRequest.put("level", level);
             jsonRequest.put("service", service);
             
             /*Url object for intialize a req*/
             URL url;
	     url = new URL(URLConnectionString+"/ProductUI/productSummary/NetOceanWebService/applyRTC");
	     
	    logger.log(Level.INFO, "creating a service url for apply RTC. "+  url);
	    HttpURLConnection conn = (HttpURLConnection) url.openConnection();
          
	    conn.setRequestMethod("POST");
        
	    conn.setRequestProperty("Accept", "application/json");
        
	    String json = jsonRequest.toString();
	    conn.setDoOutput(true);
	    OutputStream os = conn.getOutputStream();
	    os.write(json.getBytes());
	    os.flush();

	    if (conn.getResponseCode() != 200) {
	      throw new RuntimeException("Failed in Exporting a service : HTTP error code : "+ conn.getResponseCode());
	    }

            BufferedReader br = new BufferedReader(new InputStreamReader((conn.getInputStream())));
            resonseReportObj =  (JSONObject) JSONSerializer.toJSON(br.readLine());
	    logger.log(Level.INFO, "Response for getting Json service   -> "+resonseReportObj);
            
            
              
            jsonResponse  =  (JSONObject) JSONSerializer.toJSON(resonseReportObj);
            
            logger.log(Level.INFO, "Response for getting Json report   -> "+jsonResponse);
            
            String status = (String)jsonResponse.get("status"); 
             
           return status;
            
        } catch (Exception e) {
        	logger.log(Level.SEVERE, "Unknown exception. IOException -", e);
             return "Error in apply RTC.";
          }
    }
    
    
         

}
