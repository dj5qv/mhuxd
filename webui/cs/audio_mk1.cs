

<?cs def:audio_mk1_cfg1(unit, chan, keyer_type) ?>



<table class="sectiontable" cellpadding="3" cellspacing="0" width="100%">
  <tbody>


    <tr>
      <td class="titlelistcell">Mode</td>

      <td class="titlelistcell">Receive</td>
      <td class="titlelistcell">Transmit</td>
      <td class="titlelistcell">Transmit with Footswitch</td>
    </tr>


    <tr class="contentlistrow2">
      <td class="contentlistcellbold">CW</td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Cw.audioRx", 
	     mhuxd.webui.options[keyer_type].cw.audioRx, 
	     mhuxd.keyer[unit].param.r1FrBase_Cw.audioRx ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Cw.audioTx", 
	     mhuxd.webui.options[keyer_type].cw.audioTx, 
	     mhuxd.keyer[unit].param.r1FrBase_Cw.audioTx ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Cw.audioTxFootSw", 
	     mhuxd.webui.options[keyer_type].cw.audioTxFootSw, 
	     mhuxd.keyer[unit].param.r1FrBase_Cw.audioTxFootSw ) 
	     ?>
      </td>
    </tr>



    <tr class="contentlistrow2">
      <td class="contentlistcellbold">Voice</td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Voice.audioRx", 
	     mhuxd.webui.options[keyer_type].voice.audioRx, 
	     mhuxd.keyer[unit].param.r1FrBase_Voice.audioRx ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Voice.audioTx", 
	     mhuxd.webui.options[keyer_type].voice.audioTx, 
	     mhuxd.keyer[unit].param.r1FrBase_Voice.audioTx ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Voice.audioTxFootSw", 
	     mhuxd.webui.options[keyer_type].voice.audioTxFootSw, 
	     mhuxd.keyer[unit].param.r1FrBase_Voice.audioTxFootSw ) 
	     ?>
      </td>
    </tr>

    <tr class="contentlistrow2">
      <td class="contentlistcellbold">Digital</td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Digital.audioRx", 
	     mhuxd.webui.options[keyer_type].digital.audioRx, 
	     mhuxd.keyer[unit].param.r1FrBase_Digital.audioRx ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Digital.audioTx", 
	     mhuxd.webui.options[keyer_type].digital.audioTx, 
	     mhuxd.keyer[unit].param.r1FrBase_Digital.audioTx ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_select_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrBase_Digital.audioTxFootSw", 
	     mhuxd.webui.options[keyer_type].digital.audioTxFootSw, 
	     mhuxd.keyer[unit].param.r1FrBase_Digital.audioTxFootSw ) 
	     ?>
      </td>
    </tr>


  </tbody>
</table>



<?cs /def ?>

<?cs def:audio_mk1_cfg(unit, chan) ?>

<?cs call:audio_mk1_cfg1(unit, chan, mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>

<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>




<?cs /def ?>
