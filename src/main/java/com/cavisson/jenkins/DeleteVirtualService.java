package com.cavisson.jenkins;

import java.io.IOException;
import java.io.PrintStream;
import java.util.Collections;
import java.util.Map;

import org.kohsuke.stapler.DataBoundConstructor;
import org.kohsuke.stapler.StaplerRequest;

import com.cavisson.jenkins.NetStormBuilder.Descriptor;

import hudson.Extension;
import hudson.FilePath;
import hudson.Launcher;
import hudson.model.AbstractBuild;
import hudson.model.AbstractProject;
import hudson.model.FreeStyleProject;
import hudson.model.Result;
import hudson.model.Run;
import hudson.model.TaskListener;
import hudson.model.Descriptor.FormException;
import hudson.tasks.BuildStepDescriptor;
import hudson.tasks.Builder;
import jenkins.tasks.SimpleBuildStep;
import net.sf.json.JSONObject;

public class DeleteVirtualService extends Builder implements SimpleBuildStep {

	private final String serviceName;
	private String API = "/ProductUI/cavservices/virtualize/v4/deleteservice?token="+ CreateVirtualService.TOKEN;
	private String URLPath = CreateVirtualService.HOST + API;

	@DataBoundConstructor
	public DeleteVirtualService(String serviceName) {
		this.serviceName = serviceName;
	}

	@Override
	public void perform(Run<?, ?> run, FilePath workspace, Launcher launcher, TaskListener listener)
			throws InterruptedException, IOException {
		PrintStream logg = listener.getLogger();
		try {

			org.json.simple.JSONObject obj = new org.json.simple.JSONObject();
			obj.put("serviceName", serviceName);
			logg.println("URLPath : " + URLPath);
			logg.println("Config : " + obj.toString());

			String data = CreateVirtualService.getHttpResponse("POST", URLPath, obj.toString());
			if (data != null && !data.equals("")) {
				logg.println("Response : " + data);
				run.setResult(Result.SUCCESS);
			} else {
				logg.println("Services Fail" + data);
				run.setResult(Result.FAILURE);
			}

		} catch (Exception e) {
			System.out.println("Exception in writing in file - " + e);
		}

	}

	@Override
	public Descriptor getDescriptor() {
		return (Descriptor) super.getDescriptor();
	}

	@Extension
	public static class Descriptor extends BuildStepDescriptor<Builder> {
		public Descriptor() {
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
		public Builder newInstance(StaplerRequest req, JSONObject formData) throws FormException {
			return super.newInstance(req, formData); // To change body of overridden methods use File | Settings | File
														// Templates.
		}

		@Override
		public String getDisplayName() {
			return Messages.DeleteVirtualService_Task();
		}
	}

}
