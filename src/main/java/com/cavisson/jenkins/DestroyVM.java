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
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collection;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;
import net.sf.json.JSONObject;
import net.sf.json.*;
import jenkins.tasks.SimpleBuildStep;
import net.sf.json.JSONSerializer;
import net.sf.json.JSONArray;

import com.google.auth.oauth2.AccessToken;
import com.google.auth.oauth2.GoogleCredentials;
import com.google.auth.oauth2.ServiceAccountCredentials;

import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.QueryParameter;
import org.kohsuke.stapler.StaplerRequest;
import org.kohsuke.stapler.bind.JavaScriptMethod;

public class DestroyVM extends Builder implements SimpleBuildStep {
	private String INSTANCE_NAME = "";
	private String GCP_PROJECT_ID = "";
	private String IMAGE_NAME = "";

	private transient static final Logger logger = Logger.getLogger(NetStormBuilder.class.getName());


	@DataBoundConstructor
	public DestroyVM(String INSTANCE_NAME,String GCP_PROJECT_ID, String IMAGE_NAME) {
		this.INSTANCE_NAME = INSTANCE_NAME;
		this.GCP_PROJECT_ID = GCP_PROJECT_ID;
		this.IMAGE_NAME = IMAGE_NAME;
	}


	public String getINSTANCE_NAME() {
		return INSTANCE_NAME;
	}



	public void setINSTANCE_NAME(String iNSTANCE_NAME) {
		INSTANCE_NAME = iNSTANCE_NAME;
	}



	public String getGCP_PROJECT_ID() {
		return GCP_PROJECT_ID;
	}



	public void setGCP_PROJECT_ID(String gCP_PROJECT_ID) {
		GCP_PROJECT_ID = gCP_PROJECT_ID;
	}



	public String getIMAGE_NAME() {
		return IMAGE_NAME;
	}



	public void setIMAGE_NAME(String iMAGE_NAME) {
		IMAGE_NAME = iMAGE_NAME;
	}



	@Override
	public void perform(Run<?, ?> run, FilePath fp, Launcher lnchr, TaskListener taskListener) throws InterruptedException, IOException {

		PrintStream consoleLog = taskListener.getLogger();

		File credentialsPath = new File("/tmp/key.json");

		GoogleCredentials credential;
		try (FileInputStream serviceAccountStream = new FileInputStream(credentialsPath)) {
			credential = ServiceAccountCredentials.fromStream(serviceAccountStream);
		}

		Collection<String> scope = new ArrayList<String>();
		scope.add("https://www.googleapis.com/auth/cloud-platform");
		credential = credential.createScoped(scope);
		AccessToken token = credential.refreshAccessToken();
		String tokn = token.getTokenValue();

		consoleLog.println("Going to stop VM.");
		String urladd = "https://www.googleapis.com/compute/v1/projects/" + GCP_PROJECT_ID + "/zones/us-central1-a/instances/" + INSTANCE_NAME + "/stop";
		logger.log(Level.INFO, "VM stop url = " + urladd);
		URL url = new URL(urladd);
		HttpURLConnection conn = (HttpURLConnection) url.openConnection();
		conn.setConnectTimeout(0);
		conn.setReadTimeout(0);
		conn.setRequestMethod("POST");
		conn.setRequestProperty("Content-Type", "application/json");
		conn.setRequestProperty("Authorization", "Bearer " + tokn);

		// String json =payload.toString();
		conn.setDoOutput(true);

		BufferedWriter httpRequestBodyWriter = new BufferedWriter(new
				OutputStreamWriter(conn.getOutputStream()));

		httpRequestBodyWriter.write("");
		httpRequestBodyWriter.close();

		if (conn.getResponseCode() != 200) {
			logger.log(Level.INFO, "Error in stopping VM.");
			logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
			throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
		}

		consoleLog.println("Successfully stopped VM on Google Cloud... " + INSTANCE_NAME);

		TimeUnit.SECONDS.sleep(40);
		deletingVM(tokn, consoleLog);
	}

	public void deletingVM(String tokn, PrintStream consoleLog){
		try {
			consoleLog.println("Going to delete VM.");	
			String urladd = "https://www.googleapis.com/compute/v1/projects/" + GCP_PROJECT_ID + "/zones/us-central1-a/instances/" + INSTANCE_NAME;
			URL url = new URL(urladd);
			HttpURLConnection conn = (HttpURLConnection) url.openConnection();
			conn.setConnectTimeout(0);
			conn.setReadTimeout(0);
			conn.setRequestMethod("DELETE");
			conn.setRequestProperty("Content-Type", "application/json");
			conn.setRequestProperty("Authorization", "Bearer " + tokn);

			// String json =payload.toString();
			conn.setDoOutput(true);
			if (conn.getResponseCode() != 200) {
				logger.log(Level.INFO, "Error in deleting VM.");
				logger.log(Level.INFO, "Getting Error code = " + conn.getResponseCode());
				throw new RuntimeException("Failed : HTTP error code : "+ conn.getResponseCode());
			}
			consoleLog.println("Successfully deleted VM on Google Cloud: " + INSTANCE_NAME);
		} catch(Exception e) {
			logger.log(Level.INFO, "Exception = " + e);
		}

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
			return Messages.DestroyVM_Task();
		}

	}
}
