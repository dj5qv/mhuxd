

<?cs def:audio_mk2r_r2_cfg1(unit, chan, keyer_type) ?>

<?cs call:sectionheader("Audio Switching Radio 2", "foobar_help") ?>
&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="3" cellspacing="0" width="100%">
  <tbody>


    <tr>
      <td class="titlelistcell">Mode</td>

      <td class="titlelistcell">Receive</td>
      <td class="titlelistcell">Mic</td>
      <td class="titlelistcell">Transmit</td>
      <td class="titlelistcell">Mic</td>
      <td class="titlelistcell">Transmit with Footswitch</td>
      <td class="titlelistcell">Mic</td>
      <td class="titlelistcell">Codec</td>
    </tr>


    <tr class="contentlistrow2">
      <td class="contentlistcellbold">CW</td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Cw.audioRx", 
	     mhuxd.webui.options[keyer_type].cw.audioRx, 
	     mhuxd.keyer[unit].param.r2FrBase_Cw.audioRx ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Cw.audioRxMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Cw.audioRxMic ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Cw.audioTx", 
	     mhuxd.webui.options[keyer_type].cw.audioTx, 
	     mhuxd.keyer[unit].param.r2FrBase_Cw.audioTx ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Cw.audioTxMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Cw.audioTxMic) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Cw.audioTxFootSw", 
	     mhuxd.webui.options[keyer_type].cw.audioTxFootSw, 
	     mhuxd.keyer[unit].param.r2FrBase_Cw.audioTxFootSw ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Cw.audioTxFootSwMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Cw.audioTxFootSwMic ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Cw.voiceCodec", 
	     mhuxd.webui.options.mk2r_codecs, 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Cw.voiceCodec ) 
	     ?>
      </td>
    </tr>

    <tr class="contentlistrow2">
      <td class="contentlistcellbold">Voice</td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Voice.audioRx", 
	     mhuxd.webui.options[keyer_type].voice.audioRx, 
	     mhuxd.keyer[unit].param.r2FrBase_Voice.audioRx ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Voice.audioRxMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Voice.audioRxMic ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Voice.audioTx", 
	     mhuxd.webui.options[keyer_type].voice.audioTx, 
	     mhuxd.keyer[unit].param.r2FrBase_Voice.audioTx ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Voice.audioTxMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Voice.audioTxMic ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Voice.audioTxFootSw", 
	     mhuxd.webui.options[keyer_type].voice.audioTxFootSw, 
	     mhuxd.keyer[unit].param.r2FrBase_Voice.audioTxFootSw ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Voice.audioTxFootSwMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Voice.audioTxFootSwMic ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Voice.voiceCodec", 
	     mhuxd.webui.options.mk2r_codecs, 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Voice.voiceCodec ) 
	     ?>
      </td>
    </tr>

    <tr class="contentlistrow2">
      <td class="contentlistcellbold">Digital</td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Digital.audioRx", 
	     mhuxd.webui.options[keyer_type].digital.audioRx, 
	     mhuxd.keyer[unit].param.r2FrBase_Digital.audioRx ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Digital.audioRxMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Digital.audioRxMic ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Digital.audioTx", 
	     mhuxd.webui.options[keyer_type].digital.audioTx, 
	     mhuxd.keyer[unit].param.r2FrBase_Digital.audioTx ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Digital.audioRxMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Digital.audioRxMic ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrBase_Digital.audioTxFootSw", 
	     mhuxd.webui.options[keyer_type].digital.audioTxFootSw, 
	     mhuxd.keyer[unit].param.r2FrBase_Digital.audioTxFootSw ) 
	     ?>
      </td>
      <td width="5%" class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Digital.audioRxMic", 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Digital.audioRxMic ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r2FrMokExtra_Digital.voiceCodec", 
	     mhuxd.webui.options.mk2r_codecs, 
	     mhuxd.keyer[unit].param.r2FrMokExtra_Digital.voiceCodec ) 
	     ?>
      </td>
    </tr>


  </tbody>
</table>



<?cs /def ?>

<?cs def:audio_mk2r_r2_cfg(unit, chan) ?>

<?cs call:audio_mk2r_r2_cfg1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>




<?cs /def ?>
