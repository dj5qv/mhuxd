## flrig Integration

**Protocol:** XML-RPC (XML over HTTP)  
**Default Port:** 12345  
**Key Advantage:** Internal caching reduces CI-V bus traffic; handles complex VFO logic internally.

### XML-RPC Method Mapping
The following methods are called via XML-RPC to update the keyer display:


| microHAM Field | flrig XML-RPC Method | Return Type |
| :--- | :--- | :--- |
| **vfoAFreq** | `rig.get_vfoA` | double (Hz) |
| **vfoBFreq** | `rig.get_vfoB` | double (Hz) |
| **rxFreq** | `rig.get_freq` | double (Hz) |
| **Split Status**| `rig.get_split` | int (0 or 1) |
| **Active VFO** | `rig.get_vfo` | string ("VFO A" or "VFO B") |
| **Mode** | `rig.get_mode` | string (e.g., "USB", "CW-L") |

### Implementation Note
Unlike the `rigctld` workaround, `flrig` provides the active VFO name directly via `rig.get_vfo`. This makes identifying the transmit VFO during split operations more reliable for rigs with limited CAT feedback.

---

## 3. Comparison Table for mhuxd Logic


| Feature | rigctld Implementation | flrig Implementation |
| :--- | :--- | :--- |
| **VFO Selection** | Manual comparison (`f` vs `f VFOA`) | Explicit (`rig.get_vfo`) |
| **Data Parsing** | String/Regex parsing | XML-RPC structure |
| **Bus Traffic** | Polling triggers CAT commands | Polling usually hits flrig cache |
| **Backend** | Community-maintained (Hamlib) | Integrated in fldigi suite |
