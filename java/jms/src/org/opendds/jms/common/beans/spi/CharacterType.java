/*
 * $Id$
 */
 
package org.opendds.jms.common.beans.spi;

import org.opendds.jms.common.beans.UnsupportedTypeException;
import org.opendds.jms.common.lang.Strings;

/**
 * @author  Steven Stallion
 * @version $Revision$
 */
public class CharacterType implements Type {

    public Class<Character> getType() {
        return Character.class;
    }

    public Character defaultValue() {
        return 0;
    }

    public Character valueOf(Object o) {
        if (o instanceof String) {
            String s = (String) o;
            return !Strings.isEmpty(s) ? s.charAt(0) : defaultValue();
        }

        throw new UnsupportedTypeException(o);
    }
}
