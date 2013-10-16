
<?cs def:audio_mk2_opts(unit, chan) ?>


<table class="sectiontable" cellpadding="3" cellspacing="0" width="100%">
  <tbody>

    <tr>
      <td width="40%" class="titlelistcell">&nbsp;&nbsp;</td>
      <td width="20%" class="titlelistcell">CW</td>
      <td width="20%" class="titlelistcell">Voice</td>
      <td width="20%" class="titlelistcell">Digital</td>
    </tr>

    <tr class="contentlistrow2">
      <td class="contentlistcellbold">Enable On Air Recording</td>
      <td class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Cw.onAirRecActive", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Cw.onAirRecActive ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Voice.onAirRecActive", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Voice.onAirRecActive ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Digital.onAirRecActive", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Digital.onAirRecActive ) 
	     ?>
      </td>
    </tr>

    <tr class="contentlistrow2">
      <td class="contentlistcellbold">Recording enabled by Software (Logger)</td>
      <td class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Cw.onAirRecControlByRouter", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Cw.onAirRecControlByRouter ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Voice.onAirRecControlByRouter", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Voice.onAirRecControlByRouter ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_bool_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Digital.onAirRecControlByRouter", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Digital.onAirRecControlByRouter ) 
	     ?>
      </td>
    </tr>


    <tr class="contentlistrow2">
      <td class="contentlistcellbold">Audio Monitor Level (0-25)</td>
      <td class="contentlistcell">
	<?cs call:opt_number_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Cw.pwmMonctr", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Cw.pwmMonctr ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_number_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Voice.pwmMonctr", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Voice.pwmMonctr ) 
	     ?>
      </td>
      <td class="contentlistcell">
	<?cs call:opt_number_basic(
	     "set.mhuxd.keyer."+unit+".param.r1FrMpkExtra_Digital.pwmMonctr", 
	     mhuxd.keyer[unit].param.r1FrMpkExtra_Digital.pwmMonctr ) 
	     ?>
      </td>
    </tr>


  </tbody>
</table>


&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <?cs call:opt_select("Microphone Selector", 
		 "metaset.mhuxd.webui.meta.keyer."+unit+".r1.mk2micsel", 
		 mhuxd.webui.options.mk2micsel, 
		 mhuxd.webui.meta.keyer[unit].r1.mk2micsel) 
		 ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Sound Card PTT", 
		 "set.mhuxd.keyer."+unit+".param.useAutoPtt", 
		 mhuxd.keyer[unit].param.useAutoPtt) ?>


	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Downstream over Footswitch", 
		 "set.mhuxd.keyer."+unit+".param.downstreamoverFootSw", 
		 mhuxd.keyer[unit].param.downstreamoverFootSw) ?>

	    <tr>
	      <td><img src="/static/dot.gif" alt="" height="1" width="225"></td>
	      <td><img src="/static/dot.gif" alt="" height="1" width="1"></td>
	      <td><img src="/static/dot.gif" alt="" height="1" width="225"></td>
	    </tr>

	  </tbody>
	</table>
      </td>
    </tr>
  </tbody>
</table>


<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>


<?cs /def ?>
