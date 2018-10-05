
<?cs def:serial_cfg_1(chan, ol_baud, ol_databits, ol_stopbits) ?>
<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>
	    <?cs call:opt_select("Baud Rate", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".baud", 
		 mhuxd.webui.options[ol_baud], 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].baud ) 
		 ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_select("Data Bits", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".databits", 
		 mhuxd.webui.options[ol_databits], 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].databits ) 
		 ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_select("Stop Bits", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".stopbits", 
		 mhuxd.webui.options[ol_stopbits], 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].stopbits ) 
		 ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("RTS/CTS Handshake", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".rtscts", 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].rtscts) ?>

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
<?cs /def ?>

<?cs def:radio_cfg(chan) ?>
<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>


	    <tr>
	      <td class="titlesettingscell" align="right">Rig Type:</td>
	      <td class="spacersettingscell" align="right">&nbsp;&nbsp;</td>
	      <td class="contentsettingscell">
		<?cs if:mhuxd.webui.session.Edit[chan] ?>


		<select id="sel" name="set.mhuxd.keyer.<?cs var:mhuxd.webui.session.unit ?>.channel.<?cs var:chan ?>.rigtype" 
			 onchange="rigChanged(this);">
		<?cs each:item = mhuxd.run.rigtype ?>
		<option 
		   value="<?cs var:name(item) ?>" 
		   <?cs if:mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].rigtype==name(item) ?>selected<?cs /if ?>
		   >
		  <?cs var:item.name ?></option>
		<?cs /each ?>
		</select>

		<script type="text/javascript">
		  <!--
		      function rigChanged(sel) {
		      var value = sel.options[sel.selectedIndex].value;
		      var icomaddr = document.getElementById("def_icomaddress_" + value).value;
		      if(icomaddr == 0) {
		          icomaddr = "";
		      }
		      document.getElementById("icomaddress").value = icomaddr;
		      }
		      //--></script>

		<?cs each:item = mhuxd.run.rigtype ?>
		<input type="hidden" id="def_icomaddress_<?cs var:name(item) ?>" value="<?cs var:item.icom_addr ?>" >
		<?cs /each ?>

		<?cs else ?>
		<?cs var:mhuxd.run.rigtype[mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].rigtype].name ?>
		<?cs /if ?>
	      </td>
	    </tr>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_number_id("Icom Address", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".icomaddress", 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].icomaddress, "icomaddress") ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>


	    <?cs call:opt_bool("PW1 Connected", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".icomsimulateautoinfo", 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].icomsimulateautoinfo) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_select("Digital Over Voice", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".digitalovervoicerule", 
		 mhuxd.webui.options.digitalovervoicerule, 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].digitalovervoicerule ) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Use Decoder if Connected", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".usedecoderifconnected", 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].usedecoderifconnected) ?>

	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>

	    <?cs call:opt_bool("Don't Interfere USB control", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".channel."+chan+".dontinterfereusbcontrol", 
		 mhuxd.keyer[mhuxd.webui.session.unit].channel[chan].dontinterfereusbcontrol) ?>

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
<?cs /def ?>

<?cs def:fsk_cfg(chan) ?>
<table class="sectiontable" cellpadding="0" cellspacing="0">
  <tbody>
    <tr>
      <td>
	<table class="sectiontableinner" cellpadding="0" cellspacing="0">
	  <tbody>

	    <?cs if:chan=="fsk1" ?>
	    <?cs call:opt_bool("Invert FSK", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r1InvertFsk", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r1InvertFsk) ?>
	    <?cs /if ?>
	    
            <?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has.pfsk ?>
	    <tr><td class="dividersettingscell" colspan="3"><img src="/static/dot.gif" alt="" border="0" height="1" width="1"></td></tr>
	    <?cs call:opt_bool("Pseudo FSK", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.usePFsk", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.usePFsk) ?>
	    <?cs /if ?>

	    <?cs if:chan=="fsk2" ?>
	    <?cs call:opt_bool("Invert FSK", 
		 "set.mhuxd.keyer."+mhuxd.webui.session.unit+".param.r2InvertFsk", 
		 mhuxd.keyer[mhuxd.webui.session.unit].param.r2InvertFsk) ?>
	    <?cs /if ?>

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
<?cs /def ?>


<?cs def:serial_cfg(chan) ?>

<?cs if:chan=="fsk1" || chan=="fsk2" ?>
<?cs call:serial_cfg_1(chan, "fsk_baud", "fsk_databits", "fsk_stopbits") ?>
<br>
<?cs call:fsk_cfg(chan) ?>
<?cs else ?>

<?cs call:serial_cfg_1(chan, "radio_baud", "radio_databits", "radio_stopbits") ?>
<?cs if:mhuxd.run.keyer[mhuxd.webui.session.unit].flags.has[chan].radio_support == 1 ?>
<br>
<?cs call:radio_cfg(chan) ?>
<?cs /if ?>
<?cs /if ?>


<img src="/static/dot.gif" height="4" border="0" width="1"><br>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<?cs /if ?>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>



<?cs /def ?>



