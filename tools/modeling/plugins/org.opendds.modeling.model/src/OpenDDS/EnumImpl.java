/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

package OpenDDS;

import org.eclipse.emf.common.notify.Notification;
import org.eclipse.emf.ecore.EClass;
import org.eclipse.emf.ecore.impl.ENotificationImpl;

/**
 * <!-- begin-user-doc --> An implementation of the model object '
 * <em><b>Enum</b></em>'. <!-- end-user-doc -->
 * <p>
 * The following features are implemented:
 * <ul>
 * <li>{@link OpenDDS.EnumImpl#getLabels <em>Labels</em>}</li>
 * </ul>
 * </p>
 * 
 * @generated
 */
public class EnumImpl extends TopicFieldImpl implements OpenDDS.Enum {
    /**
     * The default value of the '{@link #getLabels() <em>Labels</em>}'
     * attribute. <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @see #getLabels()
     * @generated
     * @ordered
     */
    protected static final String LABELS_EDEFAULT = null;

    /**
     * The cached value of the '{@link #getLabels() <em>Labels</em>}'
     * attribute. <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @see #getLabels()
     * @generated
     * @ordered
     */
    protected String labels = LABELS_EDEFAULT;

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    protected EnumImpl() {
        super();
    }

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    @Override
    protected EClass eStaticClass() {
        return OpenDDSPackage.Literals.ENUM;
    }

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    public String getLabels() {
        return labels;
    }

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    public void setLabels(String newLabels) {
        String oldLabels = labels;
        labels = newLabels;
        if (eNotificationRequired()) {
            eNotify(new ENotificationImpl(this, Notification.SET, OpenDDSPackage.ENUM__LABELS, oldLabels, labels));
        }
    }

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    @Override
    public Object eGet(int featureID, boolean resolve, boolean coreType) {
        switch (featureID) {
            case OpenDDSPackage.ENUM__LABELS:
                return getLabels();
        }
        return super.eGet(featureID, resolve, coreType);
    }

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    @Override
    public void eSet(int featureID, Object newValue) {
        switch (featureID) {
            case OpenDDSPackage.ENUM__LABELS:
                setLabels((String) newValue);
                return;
        }
        super.eSet(featureID, newValue);
    }

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    @Override
    public void eUnset(int featureID) {
        switch (featureID) {
            case OpenDDSPackage.ENUM__LABELS:
                setLabels(LABELS_EDEFAULT);
                return;
        }
        super.eUnset(featureID);
    }

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    @Override
    public boolean eIsSet(int featureID) {
        switch (featureID) {
            case OpenDDSPackage.ENUM__LABELS:
                return LABELS_EDEFAULT == null ? labels != null : !LABELS_EDEFAULT.equals(labels);
        }
        return super.eIsSet(featureID);
    }

    /**
     * <!-- begin-user-doc --> <!-- end-user-doc -->
     * 
     * @generated
     */
    @Override
    public String toString() {
        if (eIsProxy()) {
            return super.toString();
        }

        StringBuffer result = new StringBuffer(super.toString());
        result.append(" (labels: ");
        result.append(labels);
        result.append(')');
        return result.toString();
    }

} // EnumImpl