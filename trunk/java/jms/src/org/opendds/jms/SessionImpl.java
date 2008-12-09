/*
 * $Id$
 */

package org.opendds.jms;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

import javax.jms.BytesMessage;
import javax.jms.Destination;
import javax.jms.InvalidDestinationException;
import javax.jms.JMSException;
import javax.jms.MapMessage;
import javax.jms.Message;
import javax.jms.MessageConsumer;
import javax.jms.MessageListener;
import javax.jms.MessageProducer;
import javax.jms.ObjectMessage;
import javax.jms.Queue;
import javax.jms.QueueBrowser;
import javax.jms.Session;
import javax.jms.StreamMessage;
import javax.jms.TemporaryQueue;
import javax.jms.TemporaryTopic;
import javax.jms.TextMessage;
import javax.jms.Topic;
import javax.jms.TopicSubscriber;

import DDS.DomainParticipant;

import org.opendds.jms.persistence.DurableSubscriptionStore;

/**
 * @author  Weiqi Gao
 * @version $Revision$
 */
public class SessionImpl implements Session {
    private int acknowledgeMode;
    private boolean transacted;
    private MessageListener messageListener;

    // JMS 1.1, 4.4.1, Created producers, need to close them when the session is closed
    private List<MessageProducer> createdProducers;
    // JMS 1.1, 4.4.1, Created consumers, need to close them when the session is closed
    private List<MessageConsumer> createdConsumers;
    // JMS 1.1, 4.4.14, Asynchronous Message delivery thread
    private MessageDeliveryExecutor executor;
    private ConnectionImpl owningConnection;

    // OpenDDS stuff
    private DomainParticipant participant;

    private boolean closed;
    private Object lockForClosed;

    public SessionImpl(ConnectionImpl owningConnection, boolean transacted, int acknowledgeMode) {
        this.owningConnection = owningConnection;
        this.transacted = transacted;
        this.acknowledgeMode = acknowledgeMode;

        this.participant = owningConnection.getParticipant();

        this.closed = false;

        this.createdProducers = new ArrayList<MessageProducer>();
        this.createdConsumers = new ArrayList<MessageConsumer>();
        this.executor = new MessageDeliveryExecutor(owningConnection);
    }

    public int getAcknowledgeMode() {
        return acknowledgeMode;
    }

    public boolean getTransacted() throws JMSException {
        return transacted;
    }

    public MessageListener getMessageListener() throws JMSException {
        return messageListener;
    }

    public void setMessageListener(MessageListener messageListener) throws JMSException {
        checkClosed();
        // TODO what do a session level MessageListener do?
        this.messageListener = messageListener;
    }

    public MessageConsumer createConsumer(Destination destination) throws JMSException {
        checkClosed();
        return createConsumer(destination, null, false);
    }

    public MessageConsumer createConsumer(Destination destination,
                                          String messageSelector) throws JMSException {
        checkClosed();
        return createConsumer(destination, messageSelector, false);
    }

    public MessageConsumer createConsumer(Destination destination,
                                          String messageSelector,
                                          boolean noLocal) throws JMSException {
        checkClosed();
        validateDestination(destination);
        MessageConsumer messageConsumer = new MessageConsumerImpl(this, destination, messageSelector, noLocal);
        createdConsumers.add(messageConsumer);
        return messageConsumer;
    }

    private void validateDestination(Destination destination) throws InvalidDestinationException {
        if (!(destination instanceof TopicImpl)) {
            throw new InvalidDestinationException("An invalid destination is supplied: " + destination + ".");
        }
    }

    public MessageProducer createProducer(Destination destination) throws JMSException {
        checkClosed();
        MessageProducer messageProducer = new MessageProducerImpl(this, destination);
        createdProducers.add(messageProducer);
        return messageProducer;
    }

    public TopicSubscriber createDurableSubscriber(Topic topic,
                                                   String name) throws JMSException {
        checkClosed();
        return createDurableSubscriber(topic, name, null, false);
    }

    public TopicSubscriber createDurableSubscriber(Topic topic,
                                                   String name,
                                                   String messageSelector,
                                                   boolean noLocal) throws JMSException {
        checkClosed();
        validateDestination(topic);
        DurableMessageConsumerImpl messageConsumer = new DurableMessageConsumerImpl(this, name, topic, messageSelector, noLocal);
        owningConnection.registerDurableSubscription(name, messageConsumer);
        createdConsumers.add(messageConsumer);
        return messageConsumer;
    }

    public void unsubscribe(String name) throws JMSException {
        checkClosed();
        String clientID = owningConnection.getClientID();
        if (!owningConnection.hasDurableSubscription(name)) {
            throw new InvalidDestinationException("No durable subscription is registered for [client: " + clientID + ", topic: " + name + "]");
        }
        if (owningConnection.getDurableSubscriptionByname(name) != null) {
            throw new IllegalStateException("The durable topic " + name + " has an active subscriber in client " + clientID);
        }
        owningConnection.removeDurableSubscription(name);
        DurableSubscriptionStore store = owningConnection.getPersistenceManager().getDurableSubscriptionStore();
        store.unsubscribe(new DurableSubscription(owningConnection.getClientID(), name));
    }

    public Queue createQueue(String queueName) throws JMSException {
        throw new IllegalStateException();
    }

    public TemporaryQueue createTemporaryQueue() throws JMSException {
        throw new IllegalStateException();
    }

    public QueueBrowser createBrowser(Queue queue) throws JMSException {
        throw new IllegalStateException();
    }

    public QueueBrowser createBrowser(Queue queue,
                                      String messageSelector) throws JMSException {

        throw new UnsupportedOperationException();
    }

    public Topic createTopic(String topicName) throws JMSException {
        checkClosed();
        return new TopicImpl(topicName);
    }

    public TemporaryTopic createTemporaryTopic() throws JMSException {
        checkClosed();
        return owningConnection.createTemporaryTopic();
    }

    public Message createMessage() throws JMSException {
        checkClosed();
        return new TextMessageImpl(this);
    }

    public BytesMessage createBytesMessage() throws JMSException {
        checkClosed();
        return new BytesMessageImpl(this);
    }

    public MapMessage createMapMessage() throws JMSException {
        checkClosed();
        return new MapMessageImpl(this);
    }

    public ObjectMessage createObjectMessage() throws JMSException {
        checkClosed();
        return new ObjectMessageImpl(this);
    }

    public ObjectMessage createObjectMessage(Serializable object) throws JMSException {
        checkClosed();
        ObjectMessageImpl message = new ObjectMessageImpl(this);
        message.setObject(object);
        return message;
    }

    public StreamMessage createStreamMessage() throws JMSException {
        checkClosed();
        return new StreamMessageImpl(this);
    }

    public TextMessage createTextMessage() throws JMSException {
        checkClosed();
        return new TextMessageImpl(this);
    }

    public TextMessage createTextMessage(String text) throws JMSException {
        checkClosed();
        TextMessageImpl message = new TextMessageImpl(this);
        message.setText(text);
        return message;
    }

    public void run() {
        checkClosed();
        // TODO
    }

    public void commit() throws JMSException {
        throw new IllegalStateException();
    }

    public void rollback() throws JMSException {
        throw new IllegalStateException();
    }

    public void recover() throws JMSException {
        checkClosed();
        if (transacted) throw new IllegalStateException("recover() called on a transacted Session.");
        stopMessageDelivery();
        if (messageListener != null) {
            recoverAsync();
        } else {
            for (MessageConsumer consumer: createdConsumers) {
                MessageConsumerImpl consumerImpl = (MessageConsumerImpl) consumer;
                consumerImpl.doRecover();
            }
        }
    }

    private void recoverAsync() {
        // TODO
    }

    private void stopMessageDelivery() {
        // TODO should tell consumers to stop message delivery
    }

    public void close() throws JMSException {
        if (closed) return;
        for (MessageProducer producer: createdProducers) producer.close();
        for (MessageConsumer consumer: createdConsumers) consumer.close();
        executor.shutdown();
        this.closed = true;
    }

    public void checkClosed() {
        if (closed) throw new IllegalStateException("This Session is closed.");
    }

    Executor getMessageDeliveryExecutor() {
        return executor;
    }

    void doAcknowledge() {
        for (MessageConsumer consumer: createdConsumers) {
            MessageConsumerImpl messageConsumerImpl = (MessageConsumerImpl) consumer;
            messageConsumerImpl.doAcknowledge();
        }
    }

    void addToUnacknowledged(DataReaderHandlePair dataReaderHandlePair, MessageConsumerImpl consumer) {
        consumer.addToUnacknowledged(dataReaderHandlePair);
    }

    ConnectionImpl getOwningConnection() {
        return owningConnection;
    }

    DomainParticipant getParticipant() {
        return participant;
    }
}