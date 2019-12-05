
package com.cavisson.jenkins;

import hudson.AbortException;
import hudson.Launcher;
import hudson.model.AbstractBuild;
import hudson.model.BuildListener;
import hudson.model.StringParameterValue;
import hudson.tasks.BuildWrapper;
import java.io.IOException;
import java.util.regex.Pattern;
import org.kohsuke.stapler.DataBoundConstructor;


public class NetstormStringParameterValue extends StringParameterValue 
{
    private String keyword;

    @DataBoundConstructor
    public NetstormStringParameterValue(String name, String value)
    {
      this(name, value, null, null);
    }

    public NetstormStringParameterValue(String name, String value, String keyword, String description)
    {
        super(name , value, description);
        
        this.keyword = keyword;
    }

    public String getKeyword() 
    {
        return keyword;
    }

    public void setKeyword(String keyword)
    {
        this.keyword = keyword;
    }

    public String getValue()
    {
      return value.substring(value.lastIndexOf("_") + 1) ;
    }

    @Override
    public BuildWrapper createBuildWrapper(AbstractBuild<?, ?> build) 
    {
       System.out.println("Calling create build wrapper");
       return null;
    }

    @Override
    public int hashCode() 
    {
        final int prime = 71;
        int result = super.hashCode();
        result = prime * result;
        return result;
    }

    @Override
    public boolean equals(Object obj) 
    {
        if (this == obj) {
            return true;
        }
        if (!super.equals(obj)) {
            return false;
        }
        if (NetstormStringParameterValue.class != obj.getClass()) {
            return false;
        }
        NetstormStringParameterValue other = (NetstormStringParameterValue) obj;
        if (value == null) {
            if (other.value != null) {
                return false;
            }
        } else if (!value.equals(other.value)) {
            return false;
        }
        return true;
    }

    @Override
    public String toString() {
        return "(NetStromTestParameterValue) " + getKeyword()+ " : " + getName() + "='" + value + "'";
    }
}