package com.cavisson.jenkins;

import org.kohsuke.stapler.verb.*;
import hudson.EnvVars;
import hudson.Extension;
import hudson.FilePath;
import hudson.Launcher;
import hudson.model.AbstractBuild;
import hudson.model.AbstractProject;
import hudson.model.BuildListener;
import hudson.model.FreeStyleProject;
import hudson.model.Project;
import hudson.model.Result;
import hudson.model.Run;
import hudson.model.TaskListener;
import hudson.model.User;
import hudson.tasks.BuildStepDescriptor;
import hudson.tasks.Builder;
import hudson.util.FormValidation;
import hudson.util.ListBoxModel;

import java.io.*;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.UUID;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.Iterator;
import java.util.List;

import jenkins.tasks.SimpleBuildStep;
import net.sf.json.JSONObject;
import net.sf.json.*;

import hudson.Extension;
import hudson.util.FormValidation;
import hudson.util.Secret;
import jenkins.model.Jenkins;
import org.apache.commons.lang.StringUtils;


import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.QueryParameter;
import org.kohsuke.stapler.StaplerRequest;
import org.kohsuke.stapler.bind.JavaScriptMethod;

/**
 */
public class NetStormBuilder extends Builder implements SimpleBuildStep {

    private  final String project;
    private final String subProject;
    private final String scenario;
    private final String URLConnectionString;
    private final String username;
    private final Secret password;
    private final String testMode ;
    private final String defaultTestMode = "true";
    private transient static final Logger logger = Logger.getLogger(NetStormBuilder.class.getName());
    private final String baselineType;
    private final String pollInterval;
    public static String testRunNumber = "-1";
    public static String testCycleNumber = "";
    private static final String fileName = "jenkins_check_rule_for_NS.txt";
    private String protocol="";
    private String repoIp="";
    private String repoPort="";
    private String repoPath="";
    private String repoUsername="";
    private String repoPassword="";
    private String gitPull = "";
    String uploadFileName = "";  // file name uploaded for NC data files
    private String profile="";
    private String script="";
    private String page="";
    private String advanceSett="";
    private String urlHeader="";
    private String hiddenBox="";
    private String testProfileBox="";
    private final boolean generateReport;
    Map<String, String> envVarMap = null;
    private boolean doNotWaitForTestCompletion = false;
    private String totalusers = "";
    private String rampUpSec = "";
    private String rampupmin = "";
    private String rampuphour = "";
    private String duration = "";
    private String serverhost = "";
    private String sla = "";
    private String testName = "";
    private String scriptPath = "";
    private String rampupDuration = "";
    private String emailid = "";
    private String emailidCC = "";
    private String emailidBcc = "";
    private String testsuite = "";
    private String dataDir = "";
    private String checkRuleFileUpload = "";
    private boolean fileUpload = false;
    NetStormConnectionManager netstormConnectionManger = null;
    List<String> testsuiteList = new ArrayList<String>();
    HashMap<String, ParameterDTO> testsuiteParameterMap = new HashMap<String, ParameterDTO>();
  

    public NetStormBuilder(String URLConnectionString, String username, String password, String project,
            String subProject, String scenario, String testMode, String baselineType, String pollInterval, String protocol,
            String repoIp, String repoPort, String repoPath, String repoUsername, String repoPassword, String profile,String script,String page,String advanceSett,String urlHeader,String hiddenBox,String gitPull, boolean generateReport,String testProfileBox) {
    	 logger.log(Level.INFO, "inside a constructor..............gitpull -"+gitPull);
         logger.log(Level.INFO, "profile -"+profile+", advanceSett -"+advanceSett+", urlHeader -"+urlHeader+", hiddenBox -"+hiddenBox+", testProfileBox -"+testProfileBox);
        this.project = project;
        this.subProject = subProject;
        this.scenario = scenario;
        this.URLConnectionString = URLConnectionString;
        this.username = username;
 	    this.password = StringUtils.isEmpty(password) ? null : Secret.fromString(password);
        this.testMode = testMode;
        this.baselineType = baselineType;
        this.pollInterval = pollInterval;
        this.protocol = protocol;
        this.repoIp = repoIp;
        this.repoPort = repoPort;
        this.repoPath = repoPath;
        this.repoUsername = repoUsername;
        this.repoPassword = repoPassword;
        this.profile = profile;
        this.gitPull = gitPull; 
        this.script = script;
        this.page = page;
        this.advanceSett = advanceSett;
        this.urlHeader = urlHeader;
        this.hiddenBox = hiddenBox;
        this.testProfileBox = testProfileBox;
        this.generateReport = generateReport;
        
        logger.log(Level.INFO, "hiddenBox -"+this.hiddenBox+", testmode ="+testMode+ "testProfileBox -"+this.testProfileBox);
    }
    
    
    public NetStormBuilder(String URLConnectionString, String username, String password, String project,
            String subProject, String scenario, String testMode, String baselineType, String pollInterval, String protocol,
            String repoIp, String repoPort, String repoPath, String repoUsername, String repoPassword, String profile,String script,String page,String advanceSett,String urlHeader,String hiddenBox,String gitPull, boolean generateReport, Map<String, String> envVarMap, boolean doNotWaitForTestCompletion ,String testProfileBox) {
    	 logger.log(Level.INFO, "inside a constructor..............gitpull -"+gitPull);
         logger.log(Level.INFO, "profile -"+profile+", advanceSett -"+advanceSett+", urlHeader -"+urlHeader+", hiddenBox -"+hiddenBox+", testProfileBox -"+testProfileBox);
        this.project = project;
        this.subProject = subProject;
        this.scenario = scenario;
        this.URLConnectionString = URLConnectionString;
        this.username = username;
 	    this.password = StringUtils.isEmpty(password) ? null : Secret.fromString(password);
        this.testMode = testMode;
        this.baselineType = baselineType;
        this.pollInterval = pollInterval;
        this.protocol = protocol;
        this.repoIp = repoIp;
        this.repoPort = repoPort;
        this.repoPath = repoPath;
        this.repoUsername = repoUsername;
        this.repoPassword = repoPassword;
        this.profile = profile;
        this.gitPull = gitPull; 
        this.script = script;
        this.page = page;
        this.advanceSett = advanceSett;
        this.urlHeader = urlHeader;
        this.hiddenBox = hiddenBox;
        this.testProfileBox = testProfileBox;
        this.generateReport = generateReport;
        this.envVarMap = envVarMap;
        this.doNotWaitForTestCompletion = doNotWaitForTestCompletion;
        
        logger.log(Level.INFO, "hiddenBox -"+this.hiddenBox+", testmode ="+testMode + ", doNotWaitForTestCompletion = " + doNotWaitForTestCompletion +"testProfileBox -"+this.testProfileBox);
    }

    @DataBoundConstructor
    public NetStormBuilder(String URLConnectionString, String username, String password, String project,
            String subProject, String scenario, String testMode, String baselineType, String pollInterval, String protocol,
            String repoIp, String repoPort, String repoPath, String repoUsername, String repoPassword, String profile,
            String script,String page,String advanceSett,String urlHeader,String hiddenBox,String gitPull, boolean generateReport, String testProfileBox, boolean doNotWaitForTestCompletion,
            String totalusers, String rampUpSec, String rampupmin,String rampuphour, String duration, String serverhost, 
            String sla, String testName, String scriptPath, String  rampupDuration, String emailid, String emailidCC, String emailidBcc, String testsuite, String dataDir, String checkRuleFileUpload) {
        this.project = project;
        this.subProject = subProject;
        this.scenario = scenario;
        this.URLConnectionString = URLConnectionString;
        this.username = username;
 	    this.password = StringUtils.isEmpty(password) ? null : Secret.fromString(password);
        this.testMode = testMode;
        this.baselineType = baselineType;
        this.pollInterval = pollInterval;
        this.protocol = protocol;
        this.repoIp = repoIp;
        this.repoPort = repoPort;
        this.repoPath = repoPath;
        this.repoUsername = repoUsername;
        this.repoPassword = repoPassword;
        this.profile = profile;
        this.gitPull = gitPull; 
        this.script = script;
        this.page = page;
        this.advanceSett = advanceSett;
        this.urlHeader = urlHeader;
        this.hiddenBox = hiddenBox;
        this.testProfileBox = testProfileBox;
        this.generateReport = generateReport;
        this.doNotWaitForTestCompletion = doNotWaitForTestCompletion;
        this.totalusers = totalusers;
        this.rampUpSec = rampUpSec;
        this.rampupmin = rampupmin;
        this.rampuphour = rampuphour;
        this.duration = duration;
        this.serverhost = serverhost;
        this.sla = sla;
        this.testName = testName;
        this.scriptPath = scriptPath;
        this.rampupDuration = rampupDuration;
        this.emailid = emailid;
        this.emailidCC = emailidCC;
        this.emailidBcc = emailidBcc;
        this.testsuite = testsuite;
        this.dataDir = dataDir;
        this.checkRuleFileUpload = checkRuleFileUpload;
        
        this.setParametersValue();
        
        logger.log(Level.INFO, "hiddenBox -"+this.hiddenBox+", testmode ="+testMode + ", doNotWaitForTestCompletion = " + doNotWaitForTestCompletion + " , testProfileBox = " + testProfileBox);
    }

    public NetStormBuilder(String URLConnectionString, String username, String password, String project,
            String subProject, String scenario, String testMode, String baselineType, String pollInterval,String profile, boolean generateReport) {
    	 logger.log(Level.INFO, "inside second constructor..............");
        
        this.project = project;
        this.subProject = subProject;
        this.scenario = scenario;
        this.URLConnectionString = URLConnectionString;
        this.username = username;
 	    this.password = StringUtils.isEmpty(password) ? null : Secret.fromString(password);
        this.testMode = testMode;
        this.baselineType = baselineType;
        this.pollInterval = pollInterval;
        this.profile = profile; 
        this.generateReport = generateReport;
    }

    public String getProject() 
    {
      return project;
    }
    
    public String getDefaultTestMode()
    {
      return defaultTestMode;
    }

    public String getSubProject() {
        return subProject;
    }

    public String getScenario() {
        return scenario;
    }

    public String getURLConnectionString() {
        return URLConnectionString;
    }

    public String getUsername() {
        return username;
    }
    
    public Secret getPassword() {
         return password;
    }
   
    public String getTestMode() 
    {
      return testMode;
    } 
    
    public String getBaselineType() {
    
      return baselineType;
    }
    
    public String getPollInterval() {
    	return pollInterval;
    }
    
      
  public String getProtocol() {
		return protocol;
	}

	public String getRepoIp() {
		return repoIp;
	}

	public String getRepoPort() {
		return repoPort;
	}

	public String getRepoPath() {
		return repoPath;
	}

	public String getRepoUsername() {
		return repoUsername;
	}

	public String getRepoPassword() {
		return repoPassword;
	}

	public String getGitPull() {
		return gitPull;
	}

	public void setGitPull(String gitPull) {
		this.gitPull = gitPull;
	}
	
	public String getAdvanceSett() {
		return advanceSett;
	}

	public void setAdvanceSett(String advanceSett) {
		this.advanceSett = advanceSett;
	}
     
	public String getScript() {
		return script;
	}

	public void setScript(String script) {
		this.script = script;
	}

	public String getPage() {
		return page;
	}

	public void setPage(String page) {
		this.page = page;
	}

	public String getUrlHeader() {
		return urlHeader;
	}

	public void setUrlHeader(String urlHeader) {
		this.urlHeader = urlHeader;
	}

	public String getHiddenBox() {
		return hiddenBox;
	}

	public void setHiddenBox(String hiddenBox) {
		this.hiddenBox = hiddenBox;
	}
	
	public String getProfile() {
		return profile;
	}

	public void setProfile(String profile) {
		this.profile = profile;
	}
	
	public boolean isGenerateReport() {
		return generateReport;
	}
	
	public boolean isDoNotWaitForTestCompletion() {
		return doNotWaitForTestCompletion;
	}

	public void setDoNotWaitForTestCompletion(boolean doNotWaitForTestCompletion) {
		this.doNotWaitForTestCompletion = doNotWaitForTestCompletion;
	}
	

	public Map<String, String> getEnvVarMap() {
		return envVarMap;
	}


    public String getTotalusers() {
		return totalusers;
	}


	public void setTotalusers(String totalusers) {
		this.totalusers = totalusers;
	}


	public String getRampUpSec() {
		return rampUpSec;
	}


	public void setRampUpSec(String rampupsec) {
		this.rampUpSec = rampupsec;
	}


	public String getRampupmin() {
		return rampupmin;
	}


	public void setRampupmin(String rampupmin) {
		this.rampupmin = rampupmin;
	}


	public String getRampuphour() {
		return rampuphour;
	}


	public void setRampuphour(String rampuphour) {
		this.rampuphour = rampuphour;
	}


	public String getDuration() {
		return duration;
	}


	public void setDuration(String duration) {
		this.duration = duration;
	}


	public String getServerhost() {
		return serverhost;
	}


	public void setServerhost(String serverhost) {
		this.serverhost = serverhost;
	}


	public String getSla() {
		return sla;
	}


	public void setSla(String sla) {
		this.sla = sla;
	}


	public String getTestName() {
		return testName;
	}


	public void setTestName(String testName) {
		this.testName = testName;
	}


	public String getScriptPath() {
		return scriptPath;
	}


	public void setScriptPath(String scriptPath) {
		this.scriptPath = scriptPath;
	}


	public String getRampupDuration() {
		return rampupDuration;
	}


	public void setRampupDuration(String rampupDuration) {
		this.rampupDuration = rampupDuration;
	}


	public String getEmailid() {
		return emailid;
	}


	public void setEmailid(String emailidTo) {
		this.emailid = emailidTo;
	}


	public String getEmailidCC() {
		return emailidCC;
	}


	public void setEmailidCC(String emailIdCC) {
		this.emailidCC = emailIdCC;
	}


	public String getEmailidBcc() {
		return emailidBcc;
	}


	public void setEmailidBcc(String emailIdBcc) {
		this.emailidBcc = emailIdBcc;
	}


	public String getTestsuite() {
		return testsuite;
	}


	public void setTestsuite(String testsuite) {
		this.testsuite = testsuite;
	}


	public String getDataDir() {
		return dataDir;
	}


	public void setDataDir(String dataDir) {
		this.dataDir = dataDir;
	}


	public String getCheckRuleFileUpload() {
		return checkRuleFileUpload;
	}


	public void setCheckRuleFileUpload(String checkRuleFileUpload) {
		this.checkRuleFileUpload = checkRuleFileUpload;
	}

	public String getTestProfileBox() {
		return testProfileBox;
	}


	public void setTestProfileBox(String testProfileBox) {
		this.testProfileBox = testProfileBox;
	}


@Override
  public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener taskListener) throws InterruptedException, IOException {
	run.addAction(new NetStormStopAction(run));
	run.addAction(new NetStormStopThread(run));
	PrintStream logg = taskListener.getLogger();
       Boolean fileUpload = false;
       boolean uploadNCDataFile = false;
       String scriptName = "";

       envVarMap = run instanceof AbstractBuild ? run.getEnvironment(taskListener) : Collections.<String, String>emptyMap();
       if(envVarMap.keySet().size() > 0)
        envVarMap = ((EnvVars) envVarMap).overrideAll(((AbstractBuild<?,?>) run).getBuildVariables());
       
   StringBuffer errMsg = new StringBuffer();
   
   
//   if (run instanceof AbstractBuild) {
//	   envVarMap = run.getEnvironment(taskListener);
//	   envVarMap.overrideAll(((AbstractBuild<?,?>) run).getBuildVariables());
//   } else {
//	   envVarMap = new EnvVars();
//	   
//   }
   logger.log(Level.INFO, "before if check" + envVarMap.size());
    @SuppressWarnings("rawtypes")
      Set keyset = envVarMap.keySet();
    logger.log(Level.INFO, "key set = " + keyset.size());
     String path = "";
     String jobName = "";
     String automateScripts = "";
     String testsuiteName = "";
     String dataDir = "";
     String serverhost = "";
    // NetStormConnectionManager netstormConnectionManger = null;
     if(keyset.size() > 0) {
       netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password, project, subProject, scenario, testMode, baselineType, pollInterval,profile,hiddenBox,generateReport, doNotWaitForTestCompletion, gitPull);      
	}
     
     if( (String) envVarMap.get("Testsuite") != null) {
    	 logger.log(Level.INFO, "Test Suite Nameee = " + (String)envVarMap.get("Testsuite"));
 		testsuiteName = (String)envVarMap.get("Testsuite");
 		if(!testsuiteName.isEmpty()) {
 		logger.log(Level.INFO, "Test Suite Name = " + testsuiteName);
 		
 		testsuiteList = Arrays.asList(testsuiteName.split("\\s*,\\s*"));
 		}
     }
     else if(testProfileBox != null && !testProfileBox.isEmpty())
     {
  		logger.log(Level.INFO, "testProfileBox = " + testProfileBox);
  		
  		testsuiteList = Arrays.asList(testProfileBox.split("\\s*,\\s*"));
     } else if(scenario != null && !scenario.isEmpty()) {
    	 testsuiteList = Arrays.asList(scenario.split("\\s*,\\s*"));
     }
    
//     if( (String) envVarMap.get("Testsuite") != null) {
//    		testsuiteName = (String)envVarMap.get("Testsuite");
//    		logger.log(Level.INFO, "Test Suite Name = " + testsuiteName);
//    		
//    		testsuiteList = Arrays.asList(testsuiteName.split("\\s*,\\s*"));
     if(keyset.size() > 0 && testsuiteList !=null)
     {
    		testsuiteParameterMap = new HashMap<String, ParameterDTO>();
    		logger.log(Level.INFO, "testsuite size  = " + testsuiteList.size());
    		if(testsuiteList.size() > 1) {
    			for(int i=0; i < testsuiteList.size(); i++) {
    				String suiteName = testsuiteList.get(i);
    				String prefix[] = suiteName.split("_");
    				if(prefix.length > 1)  {
    					logger.log(Level.INFO, "testsuiteParameterMap = " + testsuiteParameterMap);
    				  testsuiteParameterMap.put(prefix[0], new ParameterDTO());
    				}
    			}
    		} else if(testsuiteList.size() == 1) {
    		
    			logger.log(Level.INFO,"testsuiteList.get(0) = " + testsuiteList.get(0));
    			String[] testsuite = testsuiteList.get(0).split("/");
    			if(testsuite.length == 3) {
    				netstormConnectionManger.setProject(testsuite[0]);
    				netstormConnectionManger.setSubProject(testsuite[1]);
    				netstormConnectionManger.setScenario(testsuite[2]);
    			} else
    				netstormConnectionManger.setScenario(testsuite[0]);
    		}
      }
     
      for(Object keys : keyset)
      {
    	  
        Object value = envVarMap.get(keys);
        String key = (String) keys;
       	
       	if(key.equals("JENKINS_HOME")) {
 	     path = (String)envVarMap.get(key);
       	}
       	
       	logger.log(Level.INFO, "keys loop = " + key);
       	
       	
       	if(key.endsWith("DataDirectory")) {
       	logger.log(Level.INFO, "data dir = " + (String)envVarMap.get(key));
      	  dataDir = (String)envVarMap.get(key);
      	  
      	  if(testsuiteList.size() > 1) {
      		  String prefix[] = key.split("_");
      		  if(prefix.length > 1) {
      			  if(testsuiteParameterMap.containsKey(prefix[0]))
      				  testsuiteParameterMap.get(prefix[0]).setDataDir(dataDir);
      			  else {
      				  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
      					  testsuiteParameterMap.get(entry.getKey()).setDataDir(dataDir);	
      				  }
      			  }
      		  } else {
      			  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
      				  testsuiteParameterMap.get(entry.getKey()).setDataDir(dataDir);	
      			  }
      		  }
      	  } else if(testsuiteList.size() == 1)
      	     netstormConnectionManger.setDataDir(dataDir);
        }

        if(key.equals("Override DataDir")) {
          dataDir = (String)envVarMap.get(key);
          if(!dataDir.equals(""))
        	 netstormConnectionManger.setDataDir(dataDir);
        }
        
        if(key.endsWith("Server_Host")) {
            serverhost = (String)envVarMap.get(key);
            if(testsuiteList.size() > 1) {
        		  String prefix[] = key.split("_");
        		  if(prefix.length > 1) {
        			  
        			  if(testsuiteParameterMap.containsKey(prefix[0]))
          				  testsuiteParameterMap.get(prefix[0]).setServerhost(serverhost);
          			  else {
          				  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
          					  testsuiteParameterMap.get(entry.getKey()).setServerhost(serverhost);
          				  }
          			  }
          		  } else {
          			  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
          				  testsuiteParameterMap.get(entry.getKey()).setServerhost(serverhost);	
          			  }
          		  }
        		  
        	  } else if(testsuiteList.size() == 1)
          	     netstormConnectionManger.setServerHost(serverhost);
          }
          
       			
       			
        if(value instanceof String)
        {
           String envValue = (String) value;
           
           logger.log(Level.INFO, "env value = " + envValue);
           
           /*Need script name for updating data files*/
           if(((String) key).startsWith("Script Name")) {
              scriptName = envValue;
           }
        //   netstormConnectionManger.addSLAValue("1" , "2");
           if(envValue.startsWith("NS_SESSION"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 2)
             {
            	 
            	 if(testsuiteList.size() > 1) {
             		  String prefix[] = key.split("_");
             		  if(prefix.length > 1) {
             			  testsuiteParameterMap.get(prefix[0]).setDuration(temp[2]);
             		  }
             	  } else if(testsuiteList.size() == 1)
                    netstormConnectionManger.setDuration(temp[2]);
             }
           }
           else if(envValue.startsWith("NS_NUM_USERS"))
           {
        	   String temp [] = envValue.split("_");
        	   if(temp.length > 3) {
        		   if(testsuiteList.size() > 1) {
        			   String prefix[] = key.split("_");
        			   if(prefix.length > 1) {
        				   testsuiteParameterMap.get(prefix[0]).setTotalusers(temp[3]);
        			   }
        		   } else if(testsuiteList.size() == 1)
        			   netstormConnectionManger.setvUsers(temp[3]);
        	   }
           }  
           else if(envValue.startsWith("NS_SERVER_HOST"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 3) {
            	 if(testsuiteList.size() > 1) {
             		  String prefix[] = key.split("_");
             		  if(prefix.length > 1) {
             			 if(testsuiteParameterMap.containsKey(prefix[0]))
             				  testsuiteParameterMap.get(prefix[0]).setServerhost(temp[3]);
             			  else {
             				  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
             					  testsuiteParameterMap.get(entry.getKey()).setServerhost(temp[3]);
             				  }
             			  }
             		  } else {
             			  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
             				  testsuiteParameterMap.get(entry.getKey()).setServerhost(temp[3]);	
             			  }
             		  }
             			
             	  } else if(testsuiteList.size() == 1)
                    netstormConnectionManger.setServerHost(temp[3]);
             }
           }  
           else if(envValue.startsWith("NS_SLA_CHANGE"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 3) {
            	 if(testsuiteList.size() > 1) {
             		  String prefix[] = key.split("_");
             		  if(prefix.length > 1) {
             			  testsuiteParameterMap.get(prefix[0]).addSLAValue(prefix[1], temp[3]);
             		  }
             	  } else if(testsuiteList.size() == 1)
                     netstormConnectionManger.addSLAValue(key.toString() , temp [3] );
             }
           }
           else if(envValue.startsWith("NS_RAMP_UP_SEC") || envValue.startsWith("NS_RAMP_UP_MIN") || envValue.startsWith("NS_RAMP_UP_HR"))
           {
             String temp [] = envValue.split("_");
             if(temp.length > 4) {
            	 if(testsuiteList.size() > 1) {
             		  String prefix[] = key.split("_");
             		  if(prefix.length > 1) {
             			  testsuiteParameterMap.get(prefix[0]).setRampUp(temp[4] + "_" + temp[3]);
             		  }
             	  } else if(testsuiteList.size() == 1)
                    netstormConnectionManger.setRampUp(temp[4] + "_" + temp[3]);
             }
           }
           else if(envValue.startsWith("NS_TNAME"))
           {
             String tName = getSubString(envValue, 2, "_");
             if(!tName.equals("")) {
            	 if(testsuiteList.size() > 1) {
             		  String prefix[] = key.split("_");
             		  if(prefix.length > 1) {
             			  testsuiteParameterMap.get(prefix[0]).setTestName(tName);
             		  }
             	  } else if(testsuiteList.size() == 1)
                    netstormConnectionManger.settName(tName);
             }
           }
           else if(envValue.startsWith("NS_AUTOSCRIPT"))
           {
        	   String temp [] = envValue.split("_", 3);
        	   if(temp.length > 2){
        		   //                netstormConnectionManger.setAutoScript(temp[2]);
        		   if(automateScripts.equals(""))
        			   automateScripts = temp[2];
        		   else
        			   automateScripts = automateScripts + "," +temp[2];
        	   }
           }
           else if(envValue.startsWith("NS_RAMP_UP_DURATION")){
        	   String temp [] = envValue.split("_");
        	   if(temp.length > 4) {
        		   if(testsuiteList.size() > 1) {
        	      		  String prefix[] = key.split("_");
        	      		  if(prefix.length > 1) {
        	      			  testsuiteParameterMap.get(prefix[0]).setRampupDuration(temp[4]);
        	      		  }
        	      	  } else if(testsuiteList.size() == 1)
        		        netstormConnectionManger.setRampUpDuration(temp[4]);
        	   }
        	 
           }

           else if(envValue.startsWith("EMAIL_IDS_TO")) {
        	   String temp [] = envValue.split("_");
        	   if(temp.length > 3)  {
        		   String mail = envValue.split("IDS_TO_")[1].replaceAll("\\|", ",");
        		   if(testsuiteList.size() > 1) {
        	      		  String prefix[] = key.split("_");
        	      		  if(prefix.length > 1) { 
        	      			if(testsuiteParameterMap.containsKey(prefix[0]))
                				  testsuiteParameterMap.get(prefix[0]).setEmailid(mail);
                			  else {
                				  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
                					  testsuiteParameterMap.get(entry.getKey()).setEmailid(mail);
                				  }
                			  }
                		  } else {
                			  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
                				  testsuiteParameterMap.get(entry.getKey()).setEmailid(mail);
                			  }
                		  }
        	      			 
        	      	  } else if(testsuiteList.size() == 1) {
        		        netstormConnectionManger.setEmailIdTo(mail);
        	      	  }
        	   }
           }
           else if(envValue.startsWith("EMAIL_IDS_CC")) {
        	   String temp [] = envValue.split("_");
        	   if(temp.length > 3) {
        		   String mail = envValue.split("IDS_CC_")[1].replaceAll("\\|", ",");
        		   if(testsuiteList.size() > 1) {
        	      		  String prefix[] = key.split("_");
        	      		  if(prefix.length > 1) {
        	      			if(testsuiteParameterMap.containsKey(prefix[0]))
              				  testsuiteParameterMap.get(prefix[0]).setEmailidCC(mail);
              			  else {
              				  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
              					  testsuiteParameterMap.get(entry.getKey()).setEmailidCC(mail);
              				  }
              			  }
              		  } else {
              			  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
              				  testsuiteParameterMap.get(entry.getKey()).setEmailidCC(mail);
              			  }
              		  }
      	      			 
        	      	  } else if(testsuiteList.size() == 1) {
        		        netstormConnectionManger.setEmailIdCc(mail);
        	      	  }
        	   }
           }
           else if(envValue.startsWith("EMAIL_IDS_BCC")) {
        	   String temp [] = envValue.split("_");
        	   if(temp.length > 3) {
        		   String mail = envValue.split("IDS_BCC_")[1].replaceAll("\\|", ",");
        		   if(testsuiteList.size() > 1) {
        	      		  String prefix[] = key.split("_");
        	      		  if(prefix.length > 1) {
        	      			if(testsuiteParameterMap.containsKey(prefix[0]))
                				  testsuiteParameterMap.get(prefix[0]).setEmailidBcc(mail);
                			  else {
                				  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
                					  testsuiteParameterMap.get(entry.getKey()).setEmailidBcc(mail);
                				  }
                			  }
                		  } else {
                			  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
                				  testsuiteParameterMap.get(entry.getKey()).setEmailidBcc(mail);
                			  }
                		  }
        	      			
        	      	  } else if(testsuiteList.size() == 1) {
        		        netstormConnectionManger.setEmailIdBcc(mail);
        	      	  }
        	   }
           }
           
           if(envValue.equalsIgnoreCase(fileName))
           {
             fileUpload = true;
           }
        }
      }
      
      if(!automateScripts.isEmpty())
        netstormConnectionManger.setAutoScript(automateScripts);
     
      if(testMode == null)
      {
    	  logg.println("Please verify configured buit step, test profile mode is not selected.");
        run.setResult(Result.FAILURE);
        //return false;
      }
      
      
      if(scenario.equals("") || scenario == null || scenario.equals("---Select Scenarios ---")){
    	  if(getTestMode().equals("N"))
    		  logg.println("Please verify configured build step, scenario is not selected.");
    	  else
    		  logg.println("Please verify configured build step, Test Suite is not selected.");
    	  run.setResult(Result.FAILURE);
    	  return;
      }
      
      if(getTestMode().equals("N"))
    	  logg.println("Starting test with scenario(" + project + "/" + subProject + "/" + scenario + ")");
      else
    	  logg.println("Starting test with test suite(" + project + "/" + subProject + "/" + scenario + ")");
      

      logg.println("NetStorm URI: " + URLConnectionString );
      
      JSONObject json = null;
      
      if(fileUpload)
       {
    	json = createJsonForFileUpload(fp, logg);
    	 
       }
        
      netstormConnectionManger.setJkRule(json);
     
    /*If script name is given in string parameter then update data file which is uploaded on path workspace/jobname/uploadNCDataFiles/filename. */
      if(scriptName != null && !scriptName.equals("")) {
         FilePath file = new FilePath(fp.getChannel(), fp +"/uploadNCDataFiles");
                 logger.log(Level.INFO, "File path for uploading data file = " + file);
                 if(file.isDirectory())
                 {
                         logger.log(Level.INFO, "List of files = " + file.list());
                         
                   if(file.list().size() > 0) {
                       FilePath files = file.list().get(0);
                       netstormConnectionManger.updateNCDataFile(files, scriptName, username, profile,logg);
                   } else {
                       logg.println("No Data file is uploaded");
                   }
                   
                 // updateScriptFile(scriptName, qqq);
                 }
      } 
      String destDir = fp + "/TestSuiteReport";
	  
      FilePath direc = new FilePath(fp.getChannel(), destDir);
	  if(direc.exists())
		  direc.deleteRecursive(); 
	  
	  HashMap result= new HashMap();
	  if(testsuiteList != null && testsuiteList.size() > 1) {
		  netstormConnectionManger.setTestsuitelist(testsuiteList);
		  netstormConnectionManger.setTestsuiteParameterDTO(testsuiteParameterMap);
		  result = netstormConnectionManger.startMultipleTest(errMsg ,logg, repoPath);
	  // JSONObject requestObj = getTestsuiteJson();  
	  } else{
	    result = netstormConnectionManger.startNetstormTest(errMsg ,logg, repoPath);
	  }
	 
      
      
      logger.log(Level.INFO, "result 11111 = " + result.toString());
      if(doNotWaitForTestCompletion == true) {
    	  if(result.get("TESTRUN") != null && !result.get("TESTRUN").toString().trim().equals("")) {
    	   logg.println("Test Run Number - " + result.get("TESTRUN"));
    	   run.setDisplayName((String)result.get("TESTRUN"));
    	   run.setResult(Result.SUCCESS);
    	  }
    	  return;
      }
      
      processTestResult(result, logg, fp, run, path, netstormConnectionManger);
      
      //return status;  
}

public void processTestResult(HashMap result, PrintStream logg, FilePath fp, Run<?, ?> run, String path, NetStormConnectionManager netstormConnectionManger) {
	try {
		
		boolean status = (Boolean )result.get("STATUS");
		logg.println("result - " + result.toString());
	      
	      if(result.get("TESTRUN") != null && !result.get("TESTRUN").toString().trim().equals(""))
	      {
	        try
	        {
	        	logg.println("Test Run  - " + result.get("TESTRUN"));
	          //run.set
	          run.setDisplayName((String)result.get("TESTRUN"));
	          
	          /*set a test run number in static refrence*/
	          testRunNumber = (String)result.get("TESTRUN");
	          
	          if(testMode.equals("T") && (result.get("errMsg") == null || result.get("errMsg").toString().trim().equals(""))) {
	        	  
	        	  if(result.get("TEST_CYCLE_NUMBER") != null)
	        		  testCycleNumber = (String) result.get("TEST_CYCLE_NUMBER");
	              
	            if(generateReport == true) {
	            	logg.println("generateReport  - " + generateReport);
	            	netstormConnectionManger.checkTestSuiteStatus(logg, fp, run);
	            }
	          }
	          
	 
	          if(result.get("ENV_NAME") != null && !result.get("ENV_NAME").toString().trim().equals(""))
	            run.setDescription((String)result.get("ENV_NAME")); 
	         
	          
	          //To set the host and user name in a file for using in other publisher.
	          
	          File dir = new File(path.trim()+"/Property");
	          if (!dir.exists()) {
	              if (dir.mkdir()) {
	                  System.out.println("Directory is created!");
	              } else {
	                  System.out.println("Failed to create directory!");
	              }
	          }
	          
	          File file = new File(path.trim()+"/Property/" +((String)result.get("TESTRUN")).trim()+"_CavEnv.property");
	          
	          if(file.exists())
	           file.delete();
	          else
	          {
	            file.createNewFile();
	            
	            try
	            {
	              FileWriter fw = new FileWriter(file, true);
	              BufferedWriter bw = new BufferedWriter(fw);
	              bw.write("HostName="+URLConnectionString);
	              bw.write("\n");
	              bw.write("UserName="+username);
	              bw.close();
	            }
	            catch (Exception e){
	            	 System.out.println("Exception in writing in file - "+e);
	            }
	         }
	          
	          run.setResult(Result.SUCCESS);
	        }
	        catch(Exception e)
	        {
	        	System.out.println("Unknown exception. IOException -"+e);
	        }
	      }
	      else
	       run.setResult(Result.FAILURE);
		
	}catch (Exception e) {
		e.printStackTrace();
	}
	
}


public JSONObject getTestsuiteJson() {
	try {
		JSONArray testsuiteArray = new JSONArray();
		for(int i=0; i < testsuiteList.size(); i++) {
			String prefix[] = testsuiteList.get(i).split("_");
			if(prefix.length > 1) {
				logger.log(Level.INFO, "parameter dto = " + testsuiteParameterMap.get(prefix[0]));
			 JSONObject obj = testsuiteParameterMap.get(prefix[0]).testsuiteJson();
			 obj.put("scenario", testsuiteList.get(i));
			 obj.put("project", project);
			 obj.put("subproject", subProject);
			 obj.put("testmode", testMode);
			 obj.put("scriptHeaders",hiddenBox);
			 obj.put("baselineType", baselineType);
			 testsuiteArray.add(obj);
			}
		}
		
		JSONObject requestObj = new JSONObject();
		requestObj.put("username", username);
		requestObj.put("password", password.getPlainText());
		requestObj.put("URLConnectionString", URLConnectionString);
		requestObj.put("scenario", testsuiteArray);
		requestObj.put("workProfile",profile);
	    requestObj.put("generateReport", Boolean.toString(generateReport));
	    String uniqueID = UUID.randomUUID().toString();
	    requestObj.put("JOB_ID", uniqueID);
	    return requestObj;
		
	} catch(Exception e) {
		return null;
	}
}

public void setParametersValue() {
	try {
		logger.log(Level.INFO, "project = " + project + ", scenario => " + scenario);
	    netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password, project, subProject, scenario, testMode, baselineType, pollInterval,profile,hiddenBox,generateReport, doNotWaitForTestCompletion, gitPull);      
	logger.log(Level.INFO, "parameter values testsuite = " + testsuite + ", data dir = " + dataDir);	
	  //  if(testsuiteList.size() == 1) {
	    	if(testsuite != null && !testsuite.isEmpty()) {
	    		logger.log(Level.INFO, "Test Suite Name = " + testsuite);
	    		testsuiteList = Arrays.asList(testsuite.split("\\s*,\\s*"));
	    		testsuiteParameterMap = new HashMap<String, ParameterDTO>();
	    		
	    		logger.log(Level.INFO, "testsuite size  = " + testsuiteList.size());
	    		if(testsuiteList.size() > 1) {
	    			for(int i=0; i < testsuiteList.size(); i++) {
	    				String suiteName = testsuiteList.get(i);
	    				String prefix[] = suiteName.split("_");
	    				if(prefix.length > 1)  {
	    					logger.log(Level.INFO, "testsuiteParameterMap = " + testsuiteParameterMap);
	    					testsuiteParameterMap.put(prefix[0], new ParameterDTO());
	    				}
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    			String[] testsuites = testsuite.split("/");
	    			if(testsuites.length == 3) {
	    				netstormConnectionManger.setProject(testsuites[0]);
	    				netstormConnectionManger.setSubProject(testsuites[1]);
	    				netstormConnectionManger.setScenario(testsuites[2]);
	    			} else
	    				netstormConnectionManger.setScenario(testsuite);
	    		}
	    	}

	    	if(dataDir != null && !dataDir.isEmpty()) {
	    		logger.log(Level.INFO, "data dir = " + dataDir);
	    		if(testsuiteList.size() > 1) {
	        		  List<String> dataDirNames = Arrays.asList(dataDir.split("\\s*,\\s*"));
	        		  ArrayList<String> prefixList = new ArrayList<String>();
	        		  for(int i = 0 ; i < dataDirNames.size(); i++) {
	        			  String dirName = dataDirNames.get(i);
	        			  String prefix[] = dirName.split("_");
	        			  if(prefix.length > 1) {
	        				  if(testsuiteParameterMap.containsKey(prefix[0])) {
	        					  prefixList.add(prefix[0]);
	        					  testsuiteParameterMap.get(prefix[0]).setDataDir(dirName.substring(dirName.indexOf("_") + 1, dirName.length()));
	        				  }else {
	        					  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	        						  if(!prefixList.contains(entry.getKey()))
	        						  testsuiteParameterMap.get(entry.getKey()).setDataDir(dirName.substring(dirName.indexOf("_") + 1, dirName.length()));	
	        					  }
	        				  }
	        			  }
	        		  }
	        	 } else if(testsuiteList.size() == 1)
	        	    netstormConnectionManger.setDataDir(dataDir);
	    	}


	    //	netstormConnectionManger.addSLAValue("1" , "2");
	    	if(duration != null && duration.contains("NS_SESSION"))
	    	{
	    		if(testsuiteList.size() > 1) {
	    			List<String> durationValue = Arrays.asList(duration.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < durationValue.size(); i++) {
	    				String durtn = durationValue.get(i);
	    				String prefix[] = durtn.split("_");
	    				if(prefix.length > 1) {
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						  prefixList.add(prefix[0]);
	        					  testsuiteParameterMap.get(prefix[0]).setDuration(prefix[3]);
	    					}
	        				  else {
	        					  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	        						  if(!prefixList.contains(entry.getKey()))
	        						    testsuiteParameterMap.get(entry.getKey()).setDuration(prefix[2]);
	        					  }
	        				  }
	    				}
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    			String temp [] = duration.split("_");
	    			if(temp.length > 2)
	    				netstormConnectionManger.setDuration(temp[2]);
	    		}
	    	}

	    	if(totalusers != null &&  totalusers.contains("NS_NUM_USERS"))
	    	{
	    		
	    		if(testsuiteList.size() > 1) {
	    			List<String> users = Arrays.asList(totalusers.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < users.size(); i++) {
	    				String vusers = users.get(i);
	    				String prefix[] = vusers.split("_");
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						if(prefix.length > 3) {
	    						  prefixList.add(prefix[0]);
	        					  testsuiteParameterMap.get(prefix[0]).setTotalusers(prefix[3]);
	    						}
	    					}
	        				  else {
	        					  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	        						  
	        						  if(prefix.length > 2) {
	        							  if(!prefixList.contains(entry.getKey()))
	        						       testsuiteParameterMap.get(entry.getKey()).setTotalusers(prefix[2]);
	        						  }
	        					  }
	        				  }
	 
	    			}
	    		} else if(testsuiteList.size() == 1) {

	    			String temp [] = totalusers.split("_");
	    			if(temp.length > 3)
	    				netstormConnectionManger.setvUsers(temp[3]);
	    		}
	    	}  

	    	logger.log(Level.INFO, "server host = " + serverhost);
	    	if(serverhost != null && serverhost.contains("NS_SERVER_HOST"))
	    	{
	    		logger.log(Level.INFO, "inside server host = " + serverhost);
	    		if(testsuiteList.size() > 1) {
	    			List<String> host = Arrays.asList(serverhost.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < host.size(); i++) {
	    				String shost = host.get(i);
	    				String prefix[] = shost.split("_");
	    				
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						if(prefix.length > 4) {
	    							prefixList.add(prefix[0]);
	        					  testsuiteParameterMap.get(prefix[0]).setServerhost(shost.split("HOST_")[1]);
	    						}
	    					}
	        				  else {
	        					  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	        						  
	        						  if(prefix.length > 3) {
	        							  if(!prefixList.contains(entry.getKey()))
	        						        testsuiteParameterMap.get(entry.getKey()).setServerhost(shost.split("HOST_")[1]);
	        						  }
	        					  }
	        				  }
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    			String temp [] = serverhost.split("_");
	    			if(temp.length > 3)
	    				netstormConnectionManger.setServerHost(serverhost.split("HOST_")[1]);
	    		}
	    	}  

	    	if(sla != null && sla.contains("NS_SLA_CHANGE"))
	    	{
	    		if(testsuiteList.size() > 1) {
	    			List<String> slas = Arrays.asList(sla.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < slas.size(); i++) {
	    				String checkRuleSla = slas.get(i);
	    				String prefix[] = checkRuleSla.split("_");
	    
	    				if(testsuiteParameterMap.containsKey(prefix[0])) {
    						if(prefix.length > 5) {
    							prefixList.add(prefix[0]);
        					  testsuiteParameterMap.get(prefix[0]).addSLAValue(prefix[5], prefix[4]);
    						}
    					  }
        				  else {
        					  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
        						  
        						  if(prefix.length > 4) {
        							if(!prefixList.contains(entry.getKey()))
        						     testsuiteParameterMap.get(entry.getKey()).addSLAValue(prefix[4], prefix[3]);
        						  }
        						 }
        				  }
	    			
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    		String temp [] = sla.split("_");
	    		if(temp.length > 3)
	    			netstormConnectionManger.addSLAValue(temp[4] , temp [3] );
	    		}
	    	}

	    	if(rampUpSec != null && rampUpSec.contains("NS_RAMP_UP_SEC"))
	    	{
	    		if(testsuiteList.size() > 1) {
	    			List<String> rampup = Arrays.asList(rampUpSec.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < rampup.size(); i++) {
	    				String rampupsec = rampup.get(i);
	    				String prefix[] = rampupsec.split("_");
	    				
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						if(prefix.length > 5) {
	    							prefixList.add(prefix[0]);
	        					  testsuiteParameterMap.get(prefix[0]).setRampUp(prefix[5] + "_" + prefix[4]);
	    						}
	    					 }
	        				  else {
	        					  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	        						  
	        						  if(prefix.length > 4) {
	        							if(!prefixList.contains(entry.getKey()))
	        						     testsuiteParameterMap.get(entry.getKey()).setRampUp(prefix[4] + "_" + prefix[3]);
	        						  }
	        					   }
	        				  }
	    				
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    			String temp [] = rampUpSec.split("_");
	    			if(temp.length > 4)
	    				netstormConnectionManger.setRampUp(temp[4] + "_" + temp[3]);
	    		}
	    	}

	    	if(rampupmin != null && rampupmin.contains("NS_RAMP_UP_MIN")) {
	    		if(testsuiteList.size() > 1) {
	    			
	    			List<String> rampup = Arrays.asList(rampupmin.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < rampup.size(); i++) {
	    				String rampupsec = rampup.get(i);
	    				String prefix[] = rampupsec.split("_");
	    				
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						if(prefix.length > 5) {
	    							prefixList.add(prefix[0]);
	        					  testsuiteParameterMap.get(prefix[0]).setRampUp(prefix[5] + "_" + prefix[4]);
	    						}
	    					}
	        				  else {
	        					  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	        						  
	        						  if(prefix.length > 4) {
	        							  if(!prefixList.contains(entry.getKey()))
	        						       testsuiteParameterMap.get(entry.getKey()).setRampUp(prefix[4] + "_" + prefix[3]);
	        						  }
	        					  }
	        				  }
	    				
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    			String temp [] = rampupmin.split("_");
	    			if(temp.length > 4)
	    				netstormConnectionManger.setRampUp(temp[4] + "_" + temp[3]);
	    		}
	    	}

	    	if (rampuphour != null && rampuphour.contains("NS_RAMP_UP_HR")) {
	    		if(testsuiteList.size() > 1) {
	    			List<String> rampup = Arrays.asList(rampuphour.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < rampup.size(); i++) {
	    				String rampupsec = rampup.get(i);
	    				String prefix[] = rampupsec.split("_");
	    				if(testsuiteParameterMap.containsKey(prefix[0])) {
    						if(prefix.length > 5) {
    							prefixList.add(prefix[0]);
        					  testsuiteParameterMap.get(prefix[0]).setRampUp(prefix[5] + "_" + prefix[4]);
    						}
    					 }
        				  else {
        					  for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
        						  
        						  if(prefix.length > 4) {
        							  if(!prefixList.contains(entry.getKey()));
        						       testsuiteParameterMap.get(entry.getKey()).setRampUp(prefix[4] + "_" + prefix[3]);
        						  }
        					   }
        				  }
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    			String temp [] = rampupmin.split("_");
	    			if(temp.length > 4)
	    				netstormConnectionManger.setRampUp(temp[4] + "_" + temp[3]);
	    		}
	    	}

	    	if(testName != null && testName.contains("NS_TNAME"))
	    	{	
	    		String tName = getSubString(testName, 2, "_");
	    		if(!tName.equals(""))
	    			netstormConnectionManger.settName(tName);
	    	}

	    	if(scriptPath.startsWith("NS_AUTOSCRIPT"))
	    	{
	    		String temp [] = scriptPath.split("_", 3);
	    		if(temp.length > 2)
	    			netstormConnectionManger.setAutoScript(temp[2]);
	    	}

	    	if(rampupDuration != null && rampupDuration.contains("NS_RAMP_UP_DURATION")){
	    		if(testsuiteList.size() > 1) {
	    			List<String> rampup = Arrays.asList(rampupDuration.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < rampup.size(); i++) {
	    				String rampupsec = rampup.get(i);
	    				String prefix[] = rampupsec.split("_");
	    				
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						if(prefix.length > 5) {
	    							prefixList.add(prefix[0]);
	    						  testsuiteParameterMap.get(prefix[0]).setRampupDuration(prefix[5]);
	    						}
	    					}
	    					else {
	    						for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	    							if(prefix.length > 4) {
	    								if(!prefixList.contains(entry.getKey()))
	    							     testsuiteParameterMap.get(entry.getKey()).setEmailid(rampupsec.split("UP_DURATION_")[1]);
	    							}
	    						}
	    					}
	    				
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    			String temp [] = rampupDuration.split("_");
	    			if(temp.length > 4)
	    				netstormConnectionManger.setRampUpDuration(temp[4]);
	    		}
	    	}

	    	logger.log(Level.INFO, "email id to = " + emailid);
	    	if(emailid != null && emailid.contains("EMAIL_IDS_TO")) {
	    		if(testsuiteList.size() > 1) {
	    			logger.log(Level.INFO, "inside email id = " + emailid);
	    			List<String> emailidto = Arrays.asList(emailid.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < emailidto.size(); i++) {
	    				String email = emailidto.get(i);
	    				String prefix[] = email.split("_");
	    					logger.log(Level.INFO, "insideeeee email id = " + email);
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						if(prefix.length > 4) {
	    					        prefixList.add(prefix[0]);
	    							String mail = email.split("IDS_TO_")[1].replaceAll("\\|", ",");
	    							logger.log(Level.INFO, "mail = " + mail);
	    						testsuiteParameterMap.get(prefix[0]).setEmailid(mail);
	    						}
	    					}else {
	    						for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	    							if(prefix.length > 3) {
	    								if(!prefixList.contains(entry.getKey())) {
	    									String mail = email.split("IDS_TO_")[1].replaceAll("\\|", ",");
	    									logger.log(Level.INFO, "mail = " + mail);
	    									testsuiteParameterMap.get(entry.getKey()).setEmailid(mail);
	    								}
	    							}
	    						}
	    					}
	    				
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    			String temp [] = emailid.split("_");
	    			if(temp.length > 3) {
	    				String mail = emailid.split("IDS_TO_")[1].replaceAll("\\|", ",");
	    				logger.log(Level.INFO, "mail = " + mail);
	    				netstormConnectionManger.setEmailIdTo(mail);
	    			}
	    			}
	    	}

	    	if(emailidCC != null && emailidCC.contains("EMAIL_IDS_CC")) {
	    		
	    		if(testsuiteList.size() > 1) {
	    			List<String> emailidcc = Arrays.asList(emailidCC.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < emailidcc.size(); i++) {
	    				String email = emailidcc.get(i);
	    				String prefix[] = email.split("_");
	    				if(prefix.length > 1) {
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						if(prefix.length > 4) {
	    							prefixList.add(prefix[0]);
	    							String mail = email.split("IDS_CC_")[1].replaceAll("\\|", ",");
	    						 testsuiteParameterMap.get(prefix[0]).setEmailidCC(mail);
	    						}
	    						}else {
	    						for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	    							if(prefix.length > 3) {
	    								if(prefixList.contains(entry.getKey())) {
	    									String mail = email.split("IDS_CC_")[1].replaceAll("\\|", ",");
	    									testsuiteParameterMap.get(entry.getKey()).setEmailidCC(mail);
	    								}
	    							}
	    							}
	    					}
	    				}
	    				
	    			}
	    		} else if(testsuiteList.size() == 1) {

	    			String temp [] = emailidCC.split("_");
	    			if(temp.length > 3) {
	    				String mail = emailidCC.split("IDS_CC_")[1].replaceAll("\\|", ",");
	    				netstormConnectionManger.setEmailIdCc(mail);
	    			}
	    			}
	    	}

	    	if(emailidBcc != null && emailidBcc.contains("EMAIL_IDS_BCC")) {
	    		
	    		if(testsuiteList.size() > 1) {
	    			List<String> emailidbcc = Arrays.asList(emailidBcc.split("\\s*,\\s*"));
	    			ArrayList<String> prefixList = new ArrayList<String>();
	    			for(int i = 0 ; i < emailidbcc.size(); i++) {
	    				String email = emailidbcc.get(i);
	    				String prefix[] = email.split("_");
	    				if(prefix.length > 1) {
	    					if(testsuiteParameterMap.containsKey(prefix[0])) {
	    						if(prefix.length > 4) {
	    							prefixList.add(prefix[0]);
	    							String mail = email.split("IDS_BCC_")[1].replaceAll("\\|", ",");
	    						 testsuiteParameterMap.get(prefix[0]).setEmailidBcc(mail);
	    						}
	    						}else {
	    						for (Map.Entry<String,ParameterDTO> entry : testsuiteParameterMap.entrySet()) {
	    							if(prefix.length > 3) {
	    								if(!prefixList.contains(entry.getKey())) {
	    									String mail = email.split("IDS_BCC_")[1].replaceAll("\\|", ",");
	    									testsuiteParameterMap.get(entry.getKey()).setEmailidBcc(mail);
	    								}
	    							}
	    						}
	    					}
	    				}
	    			}
	    		} else if(testsuiteList.size() == 1) {
	    		String temp [] = emailidBcc.split("_");
	    		if(temp.length > 3) {
	    			String mail = emailidBcc.split("IDS_BCC_")[1].replaceAll("\\|", ",");
	    			netstormConnectionManger.setEmailIdBcc(mail);
	    		}
	    		}
	    	}
           
           if(checkRuleFileUpload.equalsIgnoreCase(fileName))
           {
             fileUpload = true;
           }
	} catch(Exception e) {

	}
}
  
public void getGitConfigurationFromNS(){
	  try{
		  System.out.println("In method getGitConfigurationFromNS ...");
		  logger.log(Level.INFO, "In method getGitConfigurationFromNS ...");
		  
		  NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password, false, 15);
		  String res = netstormConnectionManger.getGitConfiguration();
		  logger.log(Level.INFO, "getGitConfigurationFromNS res ..."+res);
		  System.out.println("getGitConfigurationFromNS resss ..."+res);
		  if(res == null||res.equals("")||res.equals("notConfigured")){
			  logger.log(Level.INFO, "git is not configured ...");
			  repoIp = "";
			  repoPort = "";
			  repoPath = "";
			  repoUsername = "";
			  repoPassword = "";
			  protocol = "";
		  }else{
			  String[] resArr = res.split(" ");
			  if(resArr.length>8){
			//  repoIp = resArr[0];
			//  repoPort = resArr[1];
			  repoPath = resArr[0];
			  repoUsername = resArr[2];
			  repoPassword = "";
			//  protocol = resArr[8];
			  }
		  }
		  logger.log(Level.INFO, "repoPath = " + repoPath + ", repoUsername = " + repoUsername + ", repoPassword = " + repoPassword);
	  }catch(Exception e){
		  logger.log(Level.SEVERE, "Unknown exception in getGitConfigurationFromNS.", e);
	  }
}

	@JavaScriptMethod
	public String getAddedHeaders(){
		try{
			logger.log(Level.INFO, "getAddedHeaders called ...hiddenBox -"+this.hiddenBox);
			return this.hiddenBox;
		}catch(Exception e){
			logger.log(Level.SEVERE, "Unknown exception in getAddedHeaders.",e);
			return "";
		}
	}
	
	@JavaScriptMethod
	public String getTableValue() {
		try{
			logger.log(Level.INFO, "getTableValue called ...testProfileBox -"+this.testProfileBox);
			return this.testProfileBox;
		}catch(Exception e){
			logger.log(Level.SEVERE, "Unknown exception in getTableValue.",e);
			return "";
		}
	}

  /*Method is used to create json for check rule*/
  public JSONObject createJsonForFileUpload(FilePath fp, PrintStream logger) {
	  try {
		  JSONObject json = null;
		  String fileNm = fileName;
		  if(fileName.indexOf(".") != -1) {
			  String name[] = fileName.split("\\.");
			  fileNm = name[0];
		  }
		  File file = new File(fp +"/"+fileNm);
		  logger.println("File path" + file);
		  if(file.exists())
		  {
			  BufferedReader reader = new BufferedReader(new FileReader(file));
			  StringBuilder builder = new StringBuilder();
			  String line;
			  while ((line = reader.readLine()) != null) {

				  if(line.contains("GroupName") || line.contains("GraphName") ||line.contains("VectorName") || line.contains("RuleDesc"))
				  {
					  line = line.trim().replaceAll("\\s", "@@");
				  }

				  builder.append(line.trim());
			  }
			  json = (JSONObject) JSONSerializer.toJSON(builder.toString());
		  }
		  return json;
	  } catch(Exception e) {
		  System.out.println("Unknown exception. IOException -"+ e);
		  return null;
	  }
  }
    
     /*
      *  Method which is used to start a test 
      * it makes a connection with the m/c and authenticate
      *
     */
  public String  startTest(NetStormConnectionManager netstormConnectionManger) {
      try {
          StringBuffer errBuf = new  StringBuffer();
          
           File tempFile = File.createTempFile("myfile", ".tmp");
           FileOutputStream fout = new FileOutputStream(tempFile);
          PrintStream pout=new PrintStream(fout); 
          
          //NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, password,
          //project, subProject, scenario, testMode, baselineType, pollInterval);
          
          HashMap result =   netstormConnectionManger.startNetstormTest(errBuf , pout, repoPath);
          
      
        if(result.get("TESTRUN") != null && !result.get("TESTRUN").toString().trim().equals(""))
        {
          /*set a test run number in static refrence*/
          testRunNumber = (String)result.get("TESTRUN");
          testCycleNumber = (String) result.get("TEST_CYCLE_NUMBER");     
          return result.toString();
        }
      
        return result.toString();
      }catch(Exception e) {
          System.out.println("Error in startin a test"+ e);
          return "Error in starting a test";
      }
  } 
    
   
    /**      
	 * @param OrgString
	 * @param startIndex
	 * @param seperator
	 * @return
	 * ex.--  OrgString = NS_TNAME_FIRST_TEST ,startIndex = 2 ,seperator = "_" .
	 * 
	 *      ("NS_TNAME_FIRST_TEST", 2 , "_")   method returns FIRST_TEST.
	 *      
	 */
    public String getSubString(String OrgString, int startIndex, String seperator)
    {
      String f[] = OrgString.split(seperator);
      String result = "";
      if(startIndex <= f.length-1)
      {
        for(int i = startIndex ; i < f.length; i++)
	{
          if(i == startIndex)
	    result  = result + f[i] ;
          else
            result  = result + "_" + f[i]  ;
	}
      }
      return result;
    }
  
    @Override
    public Descriptor getDescriptor() {
        return (Descriptor) super.getDescriptor();
    }

 

    @Extension
    public static class Descriptor extends BuildStepDescriptor<Builder> 
    {
        public Descriptor() 
        {
            load();
        }

        @Override
        public boolean configure(StaplerRequest req, JSONObject json) throws FormException {
            
        		  save();
            return true;
        }

        @Override
        public boolean isApplicable(Class<? extends AbstractProject> jobType) {
            return FreeStyleProject.class.isAssignableFrom(jobType);
        }

        @Override
        public Builder newInstance(StaplerRequest req, JSONObject formData) throws FormException
        {
          return super.newInstance(req, formData);    //To change body of overridden methods use File | Settings | File Templates.
        }

        @Override
        public String getDisplayName() {
            return Messages.NetStormBuilder_Task();
        }
        
        /**
         * 
         * @param password
         * @return 
         */
        public FormValidation doCheckPassword(@QueryParameter String password) {
        	
            return FieldValidator.validatePassword(password);
        }
        
        /**
         * 
         * @param username
         * @return 
         */
        public FormValidation doCheckUsername(@QueryParameter final String username) {
            return FieldValidator.validateUsername(username);
        }
        
        /**
         * 
         * @param URLConnectionString
         * @return 
         */
        public FormValidation doCheckURLConnectionString(@QueryParameter final String URLConnectionString)
        {
            return FieldValidator.validateURLConnectionString(URLConnectionString);
        }
        
        @JavaScriptMethod
        public ArrayList<String> getPulledObjects(String value,String URLConnectionString,String username,String password,String project,String subProject,String testMode,String profile){
        	try{
        		logger.log(Level.INFO,"getPulledObjects args - value :"+value+",url -"+URLConnectionString+", project -"+project+", subProject -"+subProject+",testMode -"+testMode+", profile -"+profile);
        		ArrayList<String> res = new ArrayList<String>();
        		
        		StringBuffer errMsg = new StringBuffer();
        		if(!URLConnectionString.equals("")&&!URLConnectionString.equals("NA")&&!URLConnectionString.equals(" ")&&URLConnectionString != null&&!password.equals("") && !password.equals("NA")&&!password.equals(" ") && password != null&&!username.equals("") && !username.equals("NA") && !username.equals(" ") && username != null){
        			NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
        			if(value.equals("P")){
        				logger.log(Level.INFO,"in P.......");
        				res = connection.getProjectList(errMsg,profile);
        			}else if(value.equals("SP")){
        				logger.log(Level.INFO,"in SP.......");
        				res = connection.getSubProjectList(errMsg, project,profile);
        			}else if(value.equals("S")){
        				logger.log(Level.INFO,"in S.......");
        				res = connection.getScenarioList(errMsg, project, subProject, testMode, profile);
        			}
            	}
        		if(res!=null && res.size()>0){
        			for(int i=0;i<res.size();i++){
        				logger.log(Level.INFO, "res items -"+res.get(i));
        			}
        		}
        		
        		return res;
        	}catch(Exception e){
      		logger.log(Level.SEVERE,"Exception in getPulledObjects -"+e);
      		return null;
      	}
      }
        
        /**
         * 
         * @param project
         * @return 
         */
        //public FormValidation doCheckProject(@QueryParameter final String project)
        //{
        //  return FieldValidator.validateProject(project);
        //}
        
        /**
         * 
         * @param subProject
         * @return 
         */ 
        //public FormValidation doCheckSubProject(@QueryParameter final String subProject)
        //{
        //  return FieldValidator.validateSubProjectName(subProject);
        //}
        
        /**
         * 
         * @param scenario
         * @return 
         */
        //public FormValidation doCheckScenario(@QueryParameter final String scenario)
        //{
        //    return FieldValidator.validateScenario(scenario);
        //}
        
        /**
         * 
         * @param gitPull
         * @return 
         */
//        public FormValidation doCheckGitPull(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password,@QueryParameter String gitPull,@QueryParameter String project,@QueryParameter final String subProject,@QueryParameter final String testMode)
//        {
//        	logger.log(Level.INFO, "gitPull in doCheckGitPull -"+gitPull);
//        	FormValidation validationResult;
//        	
//        	if(URLConnectionString.equals("")||URLConnectionString.equals("NA")||URLConnectionString.equals(" ")||URLConnectionString == null || password.equals("") || password.equals("NA") || password.equals(" ") || password == null||username.equals("") || username.equals("NA") || username.equals(" ") || username == null){
//        		validationResult = FormValidation.warning("Specify Netstorm URL Connection, Username and Password");
//        		return validationResult;
//        	}
//        	
//        	if(gitPull.equals("true")){
//            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false);
//            JSONObject res = netstormConnectionManger.pullObjectsFromGit();
//            logger.log(Level.INFO, "project before ..."+project);
//            if(res != null && !res.isEmpty()){
//            	if(!res.get("ErrMsg").toString().equals("")){
//            		logger.log(Level.INFO, "In first check ...");
//            		validationResult = FormValidation.warning(res.get("ErrMsg").toString());
//            	}else{
//            		logger.log(Level.INFO, "In second check ...");
//            		validationResult = FormValidation.ok(res.get("msg").toString());
//            	}
//            }else{
//            	validationResult = FormValidation.warning("GIT Pull was unsuccessful.");
//            }
//            
//        	}else{
//        		validationResult = FormValidation.ok(" ");
//        	}
//            
//        	return validationResult;
//        }
        
        @JavaScriptMethod
        public JSONObject performGitpull(String URLConnectionString,String username,String password,String gitPull, String project,String subProject,String testMode)
        {
        	logger.log(Level.INFO, "performGitpull called -");
        	JSONObject status = new JSONObject();
        	
        	if(URLConnectionString.equals("")||URLConnectionString.equals("NA")||URLConnectionString.equals(" ")||URLConnectionString == null || password.equals("") || password.equals("NA") || password.equals(" ") || password == null||username.equals("") || username.equals("NA") || username.equals(" ") || username == null){
        		String temp = "Specify Netstorm URL Connection, Username and Password";
        		status.put("msg", temp);
        		status.put("color", "#CC0000");
        		return status;
        	}
        	
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
            String res = netstormConnectionManger.pullObjectsFromGit();
            logger.log(Level.INFO, "performGitpull: project before ..."+project);
            if(res != null && !res.isEmpty()){
            		status.put("msg", res);
            		status.put("color", "#C4A000");
            }else{
            	status.put("msg", "GIT Pull was unsuccessful.");
            	status.put("color", "#C4A000");
            }
            
        	return status;
        }
        
        /**
         *
         * @param URLConnectionString
         * @param username
         * @param password
         * @return
         */
	@POST
        public FormValidation doTestNetstormConnection(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password) {

            Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);	
            FormValidation validationResult;
            
            
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
            
            StringBuffer errMsg = new StringBuffer();
            
            if(netstormConnectionManger.testNSConnection(errMsg))
              validationResult = FormValidation.ok("Successfully Connected");
            else
             validationResult = FormValidation.warning("Cannot Connect to NetStorm due to :" + errMsg);
                       
            return validationResult;
        }
       
        @POST	
        public FormValidation doPullObjectsFromGit(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("profile") String profile, @QueryParameter("repoPath") String repoPath) {

            Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);	
            FormValidation validationResult;
            StringBuffer errMsg = new StringBuffer();
                        
            NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
            String res = netstormConnectionManger.pullObjectsFromGit();
            logger.log(Level.INFO, "res in doPullObjectsFromGit -"+res);
            if(res != null && !res.isEmpty()){
            		validationResult = FormValidation.warning(res);
//            }else{
//            		logger.log(Level.INFO, "In second check ...");
//            		validationResult = FormValidation.ok(res.get("msg").toString());
//            	}
            }else{
            	validationResult = FormValidation.warning("GIT Pull was unsuccessful.");
            }
            doFillProjectItems(URLConnectionString,username,password, profile);
//            ArrayList<String> projectList = netstormConnectionManger.getProjectList(errMsg);
            return validationResult;
        }
       
	@POST
        public FormValidation doTestGitConfiguration(@QueryParameter("repoPath") String repoPath,@QueryParameter("repoUsername") String repoUserName, @QueryParameter("repoPassword") String repoPassword,@QueryParameter("username") String username,@QueryParameter("password") String password,@QueryParameter("URLConnectionString") String URLConnectionString) {
		Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
        	FormValidation validationResult;
        	
        	
        	if(URLConnectionString.equals("")||URLConnectionString.equals("NA")||URLConnectionString.equals(" ")||URLConnectionString == null || password.equals("") || password.equals("NA") || password.equals(" ") || password == null||username.equals("") || username.equals("NA") || username.equals(" ") || username == null){
        		validationResult = FormValidation.warning("Specify Netstorm URL Connection, Username and Password first ...");
        		return validationResult;
        	}
        	else if(repoPath.equals("")||repoPath.equals("NA")||repoPath.equals(" ")||repoPath == null){
        		validationResult = FormValidation.warning("Repository Path can not be empty");
        		return validationResult;
        	}
        	else if(repoUserName.equals("")||repoUserName.equals("NA")||repoUserName.equals(" ")||repoUserName == null){
        		validationResult = FormValidation.warning("Repository username can not be empty");
        		return validationResult;
        	}
        	else if(repoPassword.equals("")||repoPassword.equals("NA")||repoPassword.equals(" ")||repoPassword == null){
        		validationResult = FormValidation.warning("Repository password can not be empty");
        		return validationResult;
        	}
        	
        	NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
        	JSONObject res = netstormConnectionManger.checkGitConfiguration(repoPath,repoUserName,repoPassword,"NA");
        	if(res != null && !res.isEmpty()){
        		if(res.get("errMsg").toString().equals("")){
        			validationResult = FormValidation.ok(res.get("msg").toString());
        		}else if(!res.get("errMsg").toString().equals("")){
        			validationResult = FormValidation.warning(res.get("errMsg").toString());
        		}else{
        			validationResult = FormValidation.warning("GIT configuration test failed.");
        		}
        	}else{
        		validationResult = FormValidation.warning("GIT configuration test failed.");
        	}
        	return validationResult;
        }
        
	@POST
        public FormValidation doSaveGitConfiguration(@QueryParameter("repoPath") String repoPath,@QueryParameter("repoUsername") String repoUserName, @QueryParameter("repoPassword") String repoPassword,@QueryParameter("username") String username,@QueryParameter("password") String password,@QueryParameter("URLConnectionString") String URLConnectionString) {
		Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);
        	FormValidation validationResult;
        	
        	if(URLConnectionString.equals("")||URLConnectionString.equals("NA")||URLConnectionString.equals(" ")||URLConnectionString == null || password.equals("") || password.equals("NA") || password.equals(" ") || password == null||username.equals("") || username.equals("NA") || username.equals(" ") || username == null){
        		validationResult = FormValidation.warning("Specify Netstorm URL Connection, Username and Password first ...");
        		return validationResult;
        	}
        	else if(repoPath.equals("")||repoPath.equals("NA")||repoPath.equals(" ")||repoPath == null){
        		validationResult = FormValidation.warning("Repository Path can not be empty");
        		return validationResult;
        	}
        	else if(repoUserName.equals("")||repoUserName.equals("NA")||repoUserName.equals(" ")||repoUserName == null){
        		validationResult = FormValidation.warning("Repository username can not be empty");
        		return validationResult;
        	}
        	else if(repoPassword.equals("")||repoPassword.equals("NA")||repoPassword.equals(" ")||repoPassword == null){
        		validationResult = FormValidation.warning("Repository password can not be empty");
        		return validationResult;
        	}
        	
        	NetStormConnectionManager netstormConnectionManger = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
        	String res = netstormConnectionManger.saveGitConfiguration(repoPath, repoUserName, repoPassword, "NA");
        	logger.log(Level.INFO, "res.............."+res);
        	validationResult=FormValidation.ok(res);
        	
        	return validationResult;
        }
        
     // method for git profiles............
     	@POST
        public synchronized ListBoxModel doFillProfileItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password)
        {
          Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals(""))
          {
            models.add("---Select Profile ---");   
            return models;
          }  
          
          //Making connection server to get project list
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
         
          ArrayList<String> profileList = objProject.getProfileList(errMsg);
          
          //IF project list is found null
          if(profileList == null || profileList.size() == 0)
          {
            models.add("---Select Profile ---");   
            return models;
          }
          
          for(String profile : profileList)
            models.add(profile);
          
          return models;
        }
        
        @POST 
        public synchronized ListBoxModel doFillProjectItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password,@QueryParameter("profile") final String profile)
        {
          Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals("") || profile == null || profile.trim().equals(""))
          {
            models.add("---Select Project ---");   
            return models;
          }
          
          if(profile.trim().equals("---Select Profile ---"))
          {
            models.add("---Select Project ---");   
            return models;
          }
          
          //Making connection server to get project list
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
         
          ArrayList<String> projectList = objProject.getProjectList(errMsg,profile);
          
          //IF project list is found null
          if(projectList == null || projectList.size() == 0)
          {
            models.add("---Select Project ---");   
            return models;
          }
          
          for(String project : projectList)
            models.add(project);
          
          return models;
        }        
        
        // for baseline dropdown...
        public synchronized ListBoxModel doFillBaselineTypeItems()
        {
        	ListBoxModel models = new ListBoxModel();
            models.add("Select Baseline");
            models.add("All");
            models.add("Baseline1");
            models.add("Baseline2");
            models.add("Baseline3");
            
            return models;
        }
      
	@POST
        public synchronized ListBoxModel doFillSubProjectItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("profile") final String profile, @QueryParameter("project") final String project )
        {
          Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);	
          ListBoxModel models = new ListBoxModel();
          
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals("") || project == null || project.trim().equals(""))
          {
            models.add("---Select SubProject ---");   
            return models;
          }  
            
          if(project.trim().equals("---Select Project ---"))
          {
            models.add("---Select SubProject ---");   
            return models;
          } 
          
          NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
          StringBuffer errMsg = new StringBuffer();
          ArrayList<String> subProjectList = connection.getSubProjectList(errMsg, project, profile);
          
          if(subProjectList == null || subProjectList.size() == 0)
          {
            models.add("---Select SubProject ---");   
            return models;
          }
          
          for(String subProject : subProjectList)
          {
            models.add(subProject);
          }
             
          return models;
        }
       
	@POST
        public synchronized ListBoxModel doFillScenarioItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("profile") final String profile, @QueryParameter("project") final String project, @QueryParameter("subProject") final String subProject , @QueryParameter("testMode") final String testMode )
        {
          Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);	        	
          ListBoxModel models = new ListBoxModel();
         
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals("") || project == null || project.trim().equals("") || subProject == null || subProject.trim().equals(""))
          {
            models.add("---Select Profile ---");   
            return models;
          }  
          
          if(project.trim().equals("---Select Project ---") || subProject.trim().equals("---Select SubProject ---"))
          {
            models.add("---Select SubProject ---");   
            return models;
          } 
          
          NetStormConnectionManager connection = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
          StringBuffer errMsg = new StringBuffer();
          ArrayList<String> scenariosList = connection.getScenarioList(errMsg, project, subProject, testMode, profile);
          
          if(scenariosList == null || scenariosList.size() == 0)
          {
            models.add("---Select Scenarios ---");   
            return models;
          }
          
          for(String scenarios : scenariosList)
          {
            models.add(scenarios);
          }
          
          return models;
        }
        
        public ListBoxModel doFillTestModeItems()
        {
	   
           ListBoxModel model = new ListBoxModel();
           model.add("Scenario", "N");
           model.add("Test Suite" , "T");
           
           return model;
        }
       
	@POST
        public synchronized ListBoxModel doFillScriptItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password,@QueryParameter("profile") String profile,@QueryParameter("scenario") String scenario,@QueryParameter("project") String project,@QueryParameter("subProject") String subProject,@QueryParameter("testMode") String testMode)
        {
          Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          logger.log(Level.INFO, "scriptList: url -"+URLConnectionString+",username -"+username+",password -"+password);
          logger.log(Level.INFO, "scriptList: profile -"+profile+",project -"+project+",sub project -"+subProject+",testmode -"+testMode);
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals(""))
          {
            models.add("---Select Script---");   
            return models;
          }
          
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
          
          JSONArray scriptList = objProject.getScriptList(profile,scenario,project,subProject,testMode);
          logger.log(Level.INFO, "scriptList size -"+scriptList.size());
          //IF page list is found null
          if(scriptList == null || scriptList.size() == 0)
          {
            models.add("---Select Profile ---");   
            return models;
          }          
          
          //Iterator<String> iterator = scriptList.iterator();
          //while(iterator.hasNext()) {
        //	  logger.log(Level.INFO, "pageList item -"+iterator.next());
        //	  String temp = iterator.next();
        //	  logger.log(Level.INFO, "temp item -"+temp);
        //	  models.add(temp);
         // }
          
	 for(int i=0;i<scriptList.size();i++){
	   logger.log(Level.INFO, "scriptList item -"+(String)scriptList.get(i));
	   models.add((String)scriptList.get(i));
	 }

          return models;
        }
       
	@POST
        public synchronized ListBoxModel doFillPageItems(@QueryParameter("URLConnectionString") final String URLConnectionString, @QueryParameter("username") final String username, @QueryParameter("password") String password, @QueryParameter("script") String script,@QueryParameter("profile") String profile,@QueryParameter("scenario") String scenario,@QueryParameter("project") String project,@QueryParameter("subProject") String subProject,@QueryParameter("testMode") String testMode)
        {
          Jenkins.getInstance().checkPermission(Jenkins.ADMINISTER);	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          logger.log(Level.INFO, "pageList: url -"+URLConnectionString+",username -"+username+",password -"+password);
          logger.log(Level.INFO, "pageList: profile -"+profile+",project -"+project+",sub project -"+subProject+",testmode -"+testMode);
          
          //IF creadentials are null or blank
          if(URLConnectionString == null || URLConnectionString.trim().equals("") || username == null || username.trim().equals("") || password == null || password.trim().equals("") || script.equals("---Select Script---"))
          {
            models.add("---Select Page---");   
            return models;
          }  
          
          //Making connection server to get project list
          NetStormConnectionManager objProject = new NetStormConnectionManager(URLConnectionString, username, Secret.fromString(password), false, 15);
         
          if(!script.equals("All")){
          JSONArray pageList = objProject.getPageList(script,scenario,profile,testMode,project,subProject);
          logger.log(Level.INFO, "pageList size -"+pageList.size());
          //IF page list is found null
          if(pageList == null || pageList.size() == 0)
          {
            models.add("---Select Profile ---");   
            return models;
          }
                    
          for(int i=0;i<pageList.size();i++){
        	  logger.log(Level.INFO, "pageList item -"+(String)pageList.get(i));
        	  String temp = (String)pageList.get(i);
        	  temp = temp.replace("\"", "");
        	  logger.log(Level.INFO, "temp -"+temp);
        	  models.add(temp);
          }
          }else if(script.equals("All")){
        	  logger.log(Level.INFO, "in all check ...");
              models.add("All");        	  
          }
        
          return models;
        }
        
        public synchronized ListBoxModel doFillUrlHeaderItems()
        {
        	
          ListBoxModel models = new ListBoxModel();
          StringBuffer errMsg = new StringBuffer();
          models.add("Main");
    	  models.add("Inline");
    	  models.add("ALL");
    	 
    	  return models;
        }
    }
}
