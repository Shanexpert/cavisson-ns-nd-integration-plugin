package com.cavisson.jenkins;

import java.io.Serializable;
import java.util.HashMap;
import java.util.Set;
import java.util.logging.Level;

import net.sf.json.JSONArray;
import net.sf.json.JSONObject;

public class ParameterDTO implements Serializable{

	private String totalusers = "";
	private String rampUp = "";
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
	
	private HashMap<String,String> slaValueMap =  new HashMap<String,String> ();

	public void addSLAValue(String key, String value)
	{
		slaValueMap.put(key, value);
	}
	public String getTotalusers() {
		return totalusers;
	}
	public void setTotalusers(String totalusers) {
		this.totalusers = totalusers;
	}
	public String getRampUp() {
		return rampUp;
	}
	public void setRampUp(String rampUp) {
		this.rampUp = rampUp;
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
	public void setEmailid(String emailid) {
		this.emailid = emailid;
	}
	public String getEmailidCC() {
		return emailidCC;
	}
	public void setEmailidCC(String emailidCC) {
		this.emailidCC = emailidCC;
	}
	public String getEmailidBcc() {
		return emailidBcc;
	}
	public void setEmailidBcc(String emailidBcc) {
		this.emailidBcc = emailidBcc;
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
	
	public JSONObject testsuiteJson() {
		try {
			JSONObject jsonRequest = new JSONObject();
			if(getDuration() != null && !getDuration().trim().equals(""))
		      {
		        jsonRequest.put("DURATION", getDuration());
		      }
		      
		      if(getServerhost() != null && !getServerhost().trim().equals(""))
		      {
		        jsonRequest.put("SERVER_HOST", getServerhost());
		      }
		      
		      if(getTotalusers() != null && !getTotalusers().trim().equals(""))
		      {
		        jsonRequest.put("VUSERS", getTotalusers());
		      }
		      
		      if(getRampUp() != null && !getRampUp().trim().equals(""))
		      {
		        jsonRequest.put("RAMP_UP", getRampUp());
		      }

		      if(getRampupDuration() != null && !getRampupDuration().trim().equals("")){
		    	  jsonRequest.put("RAMP_UP_DURATION", getRampupDuration());
		      }
		      
		      if(getTestName()!= null && !getTestName().trim().equals(""))
		      {
		        jsonRequest.put("TNAME", getTestName());
		      }
		      if(getScriptPath()!= null && !getScriptPath().trim().equals(""))
		      {
		        jsonRequest.put("AUTOSCRIPT", getScriptPath());
		      }
		      
		      if(getEmailid()!= null && !getEmailid().trim().equals(""))
		      {
		        jsonRequest.put("EmailIdTo", getEmailid());
		      }
		      
		      if(getEmailidCC()!= null && !getEmailidCC().trim().equals(""))
		      {
		        jsonRequest.put("EmailIdCc", getEmailidCC());
		      }
		      
		      if(getEmailidBcc()!= null && !getEmailidBcc().trim().equals(""))
		      {
		        jsonRequest.put("EmailIdBcc", getEmailidBcc());
		      }
		      
		      if(getDataDir() != null && !getDataDir().trim().equals(""))
		      {
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
		      return jsonRequest;
		} catch(Exception e) {
			return null;
		}
	}
	@Override
	public String toString() {
		return "ParameterDTO [totalusers=" + totalusers + ", rampUp=" + rampUp + ", duration=" + duration
				+ ", serverhost=" + serverhost + ", sla=" + sla + ", testName=" + testName + ", scriptPath="
				+ scriptPath + ", rampupDuration=" + rampupDuration + ", emailid=" + emailid + ", emailidCC="
				+ emailidCC + ", emailidBcc=" + emailidBcc + ", testsuite=" + testsuite + ", dataDir=" + dataDir
				+ ", slaValueMap=" + slaValueMap + "]";
	}

}
