package com.cavisson.jenkins;

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
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.ObjectOutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;
import jenkins.tasks.SimpleBuildStep;

import net.sf.json.JSONObject;

import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.QueryParameter;
import org.kohsuke.stapler.StaplerRequest;
import org.kohsuke.stapler.bind.JavaScriptMethod;

public class FetchTestAssets extends Builder implements SimpleBuildStep{

	private String username = "";
	private String profile = "";
	private String giturl = "";
	private String ipAddress = "";
	private transient static final Logger logger = Logger.getLogger(FetchTestAssets.class.getName());

	@DataBoundConstructor
	public FetchTestAssets(String username, String profile, String giturl, String ipAddress) {
		this.username = username;
		this.profile = profile;
		this.giturl = giturl;
		this.ipAddress = ipAddress;
	}

	public String getUsername() {
		return username;
	}


	public void setUsername(String username) {
		this.username = username;
	}


	public String getProfile() {
		return profile;
	}


	public void setProfile(String profile) {
		this.profile = profile;
	}


	public String getGiturl() {
		return giturl;
	}


	public void setGiturl(String giturl) {
		this.giturl = giturl;
	}

	public String getIpAddress() {
		return ipAddress;
	}

	public void setIpAddress(String ipAddress) {
		this.ipAddress = ipAddress;
	}

	@Override
	public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener taskListener) throws InterruptedException, IOException {

		PrintStream consoleLog = taskListener.getLogger();

		JSONObject data = new JSONObject();
		data.put("masterRepo", giturl);
		data.put("passType", "0");
		data.put("passValue", "");
		data.put("productType", "NS>NO");
		data.put("repoProject", "configuration");
		data.put("userName", username);
		consoleLog.println("Starting Git Clone.");
		//		String urladd = "https://10.10.50.23:4432/ProductUI/productSummary/gitRepositoryService/gitClone";
		String urladd = "https://" + ipAddress + "/ProductUI/productSummary/gitRepositoryService/gitClone";
		logger.log(Level.INFO, "Git clone Url = " + urladd);
		URL url = new URL(urladd);
		HttpURLConnection conn = (HttpURLConnection) url.openConnection();
		conn.setConnectTimeout(0);
		conn.setReadTimeout(0);
		conn.setRequestMethod("POST");
		conn.setRequestProperty("Content-Type", "application/json");

		conn.setDoOutput(true);


		BufferedWriter httpRequestBodyWriter = new BufferedWriter(new
				OutputStreamWriter(conn.getOutputStream()));

		httpRequestBodyWriter.write(data.toString());
		httpRequestBodyWriter.close();

		if (conn.getResponseCode() != 200) {
			logger.log(Level.INFO, "Error in git clone.");
			logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
			throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
		} else {

			BufferedReader br = null;
			br = new BufferedReader(new InputStreamReader(conn.getInputStream()));

			StringBuilder sb = new StringBuilder();
			String line;
			while ((line = br.readLine()) != null) {
				sb.append(line);
			}
			consoleLog.println(sb.toString());
			br.close();

			try {
				gitPullObjects(giturl, username, profile, consoleLog);
			} catch(Exception e) {
				logger.log(Level.INFO, "Exception = " + e);
			}
		}

	}

	public void gitPullObjects(String giturl, String username, String profile, PrintStream consoleLog) throws IOException{
		JSONObject data = new JSONObject();
		data.put("activeProfile", profile);
		data.put("passType", "0");
		data.put("passValue", "");
		data.put("productType", "NS>NO");
		data.put("repo", "configuration");
		data.put("userName", username);
		consoleLog.println("Starting Git Pull.");
		//		String urladd = "https://10.10.50.23:4432/ProductUI/productSummary/gitRepositoryService/gitRefresh";
		String urladd = "https://" + ipAddress +"/ProductUI/productSummary/gitRepositoryService/gitRefresh";
		logger.log(Level.INFO, "Git pull Url = " + urladd);
		URL url = new URL(urladd);
		HttpURLConnection conn = (HttpURLConnection) url.openConnection();
		conn.setConnectTimeout(0);
		conn.setReadTimeout(0);
		conn.setRequestMethod("POST");
		conn.setRequestProperty("Content-Type", "application/json");

		conn.setDoOutput(true);

		BufferedWriter httpRequestBodyWriter = new BufferedWriter(new
				OutputStreamWriter(conn.getOutputStream()));

		httpRequestBodyWriter.write(data.toString());
		httpRequestBodyWriter.close();

		if (conn.getResponseCode() != 200) {
			logger.log(Level.INFO, "Error in git pull.");
			logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
			throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
		}

		BufferedReader br = null;
		br = new BufferedReader(new InputStreamReader(conn.getInputStream()));

		StringBuilder sb = new StringBuilder();
		String line;
		while ((line = br.readLine()) != null) {
			sb.append(line);
		}
		consoleLog.println(sb.toString());
		br.close();

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
			return Messages.FetchTestAssets_Task();
		}

	}
}
