/*
 * $Id$
 */

package org.opendds.jms.util;

import java.util.Properties;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class Version {
    private static Version instance;

    public synchronized static Version getInstance() {
        if (instance == null) {
            instance = new Version(
                PropertiesHelper.forName("version.properties"));
        }
        return instance;
    }

    private PropertiesHelper helper;

    protected Version(Properties properties) {
        helper = new PropertiesHelper(properties);
    }

    public String getDDSVersion() {
        return helper.require("version.dds").getValue();
    }

    public int getDDSMajorVersion() {
        return helper.require("version.dds.major").asInt();
    }

    public int getDDSMinorVersion() {
        return helper.require("version.dds.minor").asInt();
    }

    public String getJCAVersion() {
        return helper.require("version.jca").getValue();
    }

    public int getJCAMajorVersion() {
        return helper.require("version.jca.major").asInt();
    }

    public int getJCAMinorVersion() {
        return helper.require("version.jca.minor").asInt();
    }

    public String getJMSVersion() {
        return helper.require("version.jms").getValue();
    }

    public int getJMSMajorVersion() {
        return helper.require("version.jms.major").asInt();
    }

    public int getJMSMinorVersion() {
        return helper.require("version.jms.minor").asInt();
    }

    @Override
    public String toString() {
        return "OpenDDS v" + getDDSVersion();
    }
}
