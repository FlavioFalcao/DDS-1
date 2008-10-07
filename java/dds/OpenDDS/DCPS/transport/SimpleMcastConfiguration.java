package OpenDDS.DCPS.transport;

public class SimpleMcastConfiguration
    extends SimpleUnreliableDgramConfiguration {

    SimpleMcastConfiguration(int id) {
        super(id);
    }

    void saveSpecificConfig(long cfg) {
        super.saveSpecificConfig(cfg);
        saveMcastConfig(cfg);
    }

    private native void saveMcastConfig(long cfg);

    void loadSpecificConfig(long cfg) {
        super.loadSpecificConfig(cfg);
        loadMcastConfig(cfg);
    }

    private native void loadMcastConfig(long cfg);

    private String multicastGroupAddress;
    public String getMulticastGroupAddress() { return multicastGroupAddress; }
    public void setMulticastGroupAddress(String mga) {
        multicastGroupAddress = mga;
    }

    private boolean receiver;
    public boolean isReceiver() { return receiver; }
    public void setReceiver(boolean r) { receiver = r; }

}
