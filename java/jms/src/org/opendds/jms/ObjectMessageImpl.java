package org.opendds.jms;

import javax.jms.ObjectMessage;
import javax.jms.JMSException;
import java.io.Serializable;
import java.io.ObjectOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ByteArrayInputStream;
import java.io.ObjectInputStream;
import OpenDDS.JMS.MessageBodyKind;
import OpenDDS.JMS.MessagePayload;

public class ObjectMessageImpl extends AbstractMessageImpl implements ObjectMessage {
    public ObjectMessageImpl() {
        initObjectBody();
    }

    public ObjectMessageImpl(MessagePayload messagePayload, int handle) {
        super(messagePayload, handle);
        setBodyState(new MessageStateBodyNonWritable(this));
    }

    private void initObjectBody() {
        payload.theBody.theOctetSeqBody(MessageBodyKind.OBJECT_KIND, null);
        setBodyState(new MessageStateWritable());
    }

    public void setObject(Serializable serializable) throws JMSException {
        getBodyState().checkWritable();
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        try {
            ObjectOutputStream oos = new ObjectOutputStream(baos);
            oos.writeObject(serializable);
            payload.theBody.theOctetSeqBody(baos.toByteArray());
            oos.close();
        } catch (IOException e) {
            // Can't happen
        }
    }

    public Serializable getObject() throws JMSException {
        final byte[] buf = payload.theBody.theOctetSeqBody();

        if (buf == null) return null;
        ByteArrayInputStream bais = new ByteArrayInputStream(buf);
        try {
            ObjectInputStream ois = new ObjectInputStream(bais);
            final Serializable retVal = (Serializable) ois.readObject();
            ois.close();
            return retVal;
        } catch (IOException e) {
            return null; // Can't happen
        } catch (ClassNotFoundException e) {
            return null; // TODO
        }
    }

    protected void doClearBody() {
        initObjectBody();
    }
}