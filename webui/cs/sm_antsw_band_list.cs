<?cs def:print_link_name(unit, bandid, refid) ?>
<?cs if:mhuxd.keyer[unit].sm.obj[mhuxd.keyer[unit].sm.obj[bandid].ref[refid].dest_id].type == 0 ?>
ANT: <?cs var:mhuxd.keyer[unit].sm.obj[mhuxd.keyer[unit].sm.obj[bandid].ref[refid].dest_id].display ?>
<?cs else ?>
<?cs if:mhuxd.keyer[unit].sm.obj[mhuxd.keyer[unit].sm.obj[bandid].ref[refid].dest_id].virtual_rotator == 1 ?>
VIR: <?cs var:mhuxd.keyer[unit].sm.obj[mhuxd.keyer[unit].sm.obj[bandid].ref[refid].dest_id].display ?>
<?cs else ?>
GRP: <?cs var:mhuxd.keyer[unit].sm.obj[mhuxd.keyer[unit].sm.obj[bandid].ref[refid].dest_id].display ?>
<?cs /if ?>
<?cs /if ?>
<?cs /def ?>


<?cs def:select_link_rw(name) ?>
<select name="<?cs var:name ?>" >
  <?cs each:obj_item = mhuxd.keyer[unit].sm.obj ?>
  <?cs if:obj_item.type == 0 || obj_item.type == 1 ?>
  <option value = "<?cs var:obj_item.id ?>"
	  <?cs if:mhuxd.keyer[unit].sm.obj[item.id].ref[ref_item.id].dest_id == obj_item.id ?>
	  selected = "1"
	  <?cs /if ?>
	  >

    <?cs if:obj_item.type == 0 ?>ANT : <?cs /if ?>
    <?cs if:obj_item.type == 1 && obj_item.virtual_rotator == 1 ?>VIR : <?cs /if ?>
    <?cs if:obj_item.type == 1 && obj_item.virtual_rotator == 0 ?>GRP : <?cs /if ?>
    <?cs var:obj_item.display ?>
  </option>
  <?cs /if ?>
  <?cs /each ?>
</select>
<?cs /def ?>


<?cs def:select_link(name) ?>
<?cs if:mhuxd.webui.session.Edit[chan] ?>
<?cs call:select_link_rw(name) ?>
<?cs else ?>

<?cs var:optlist[optvalue].display ?>
<?cs call:print_link_name(unit, item.id, ref_item.id) ?>
<?cs /if ?>
<?cs /def ?>



<?cs def:sm_antsw_band_list1(unit, chan) ?>

<!-- BASE PAGE -->


<?cs call:sectionheader("Band List", "foobar_help") ?>

&nbsp;&nbsp;<br>

<table class="sectiontable" cellpadding="3" cellspacing="0" width="80%">
  <tbody>
    <tr>
      <td class="titlelistcell">&nbsp;</td>
      <td class="titlelistcell">ID</td>
      <td class="titlelistcell">Name</td>
      <td class="titlelistcell" colspan="2">Frequency Range</td>
      <td class="titlelistcell">Code</td>
      <td class="titlelistcell">PA</td>
      <td class="titlelistcell">KeyOut</td>
      <td class="titlelistcell">Ant</td>
      <td class="titlelistcell">BPF</td>
      <td class="titlelistcell">SEQ</td>
    </tr>

    <?cs each:item = mhuxd.keyer[unit].sm.obj ?>
    <?cs if:item.type == 2 ?>

    <!-- main row -->
    <tr class="bandrow">
      <?cs if:mhuxd.webui.session.Edit[chan] ?>
      <td class="contentlistcell" width="19" align="center">&nbsp;</td>
      <?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".id", item.id) ?>
      <?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".type", item.type) ?>
      <?cs else ?>
      <td class="radiolistcell2" width="19" align="center">
	<input type="checkbox" name="modify.mhuxd.keyer.<?cs var:unit ?>.sm.obj.<?cs var:name(item) ?>" value="1" > 
      </td>
      <?cs /if ?>

      <td class="contentlistcell" width="1%"><?cs var:name(item) ?></td>


      <td class="contentlistcell" width="25%" ><?cs call:string_basic(
				       "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".display", 
				       mhuxd.keyer[unit].sm.obj[name(item)].display, 10 ) ?> </td>

      <td class="contentlistcell" width="25%" ><?cs call:string_basic(
				       "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".low_freq", 
				       mhuxd.keyer[unit].sm.obj[name(item)].low_freq, 10 ) ?> </td>

      <td class="contentlistcell" width="25%" ><?cs call:string_basic(
				       "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".high_freq", 
				       mhuxd.keyer[unit].sm.obj[name(item)].high_freq, 10 ) ?> </td>

      <td class="contentlistcell" width="25%" ><?cs call:string_basic(
				       "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".bcd_code", 
				       mhuxd.keyer[unit].sm.obj[name(item)].bcd_code, 1 ) ?> </td>

      <td class="contentlistcell"><?cs call:opt_bool_basic(
				       "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".pa_power", 
				       mhuxd.keyer[unit].sm.obj[name(item)].pa_power ) ?></td>

      <td class="contentlistcell"><?cs call:opt_bool_basic(
				       "modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".keyout", 
				       mhuxd.keyer[unit].sm.obj[name(item)].keyout ) ?></td>

      <td class="contentlistcell" width="1%" align="center">&nbsp;</td>

      <!-- BPF -->
      <td class="contentlistcell" width="1%" align="center">
	<table class="sectiontable" cellpadding="3" cellspacing="0">
	  <tr>
	    <?cs each:out_item = mhuxd.keyer[unit].sm.output ?>
	    <?cs if:mhuxd.keyer[unit].sm.output[name(out_item)] == 1 ?>
	    <td class="outputheadercell"><?cs var:name(out_item) ?></td>
	    <?cs /if ?>
	    <?cs /each ?>
	  </tr>

	  <tr>
	    <?cs each:out_item = mhuxd.keyer[unit].sm.output ?>
	    <?cs if:mhuxd.keyer[unit].sm.output[name(out_item)] == 1 ?>
	    <td class="contentlistcell"><?cs call:opt_bool_basic(
					     "modify.mhuxd.keyer."+unit+".sm.obj."+item.id+".bpf_seq."+name(out_item), 
					     mhuxd.keyer[unit].sm.obj[item.id].bpf_seq[name(out_item)]) ?></td>
	    <?cs /if ?>
	    <?cs /each ?>
	  </tr>
	</table>
      </td>

      <!-- SEQ -->
      <td class="contentlistcell" width="1%" align="center">
	<table class="sectiontable" cellpadding="3" cellspacing="0">
	  <tr>
	    <?cs each:out_item = mhuxd.keyer[unit].sm.output ?>
	    <?cs if:mhuxd.keyer[unit].sm.output[name(out_item)] == 2 || mhuxd.keyer[unit].sm.output[name(out_item)] == 3  ?>
	    <td class="outputheadercell"><?cs var:name(out_item) ?></td>
	    <?cs /if ?>
	    <?cs /each ?>
	  </tr>

	  <tr>
	    <?cs each:out_item = mhuxd.keyer[unit].sm.output ?>
	    <?cs if:mhuxd.keyer[unit].sm.output[name(out_item)] == 2 || mhuxd.keyer[unit].sm.output[name(out_item)] == 3 ?>
	    <td class="contentlistcell"><?cs call:opt_bool_basic(
					     "modify.mhuxd.keyer."+unit+".sm.obj."+item.id+".bpf_seq."+name(out_item), 
					     mhuxd.keyer[unit].sm.obj[item.id].bpf_seq[name(out_item)]) ?></td>
	    <?cs /if ?>
	    <?cs /each ?>
	  </tr>
	</table>
      </td>

    </tr>

    <!-- list ref area -->
    <?cs each:ref_item = mhuxd.keyer[unit].sm.obj[name(item)].ref ?>
    <tr class="refrow">
      <?cs if:mhuxd.webui.session.Edit[chan] ?>
      <td class="contentlistcell" width="19" align="center">&nbsp;</td>
      <?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".ref."+name(ref_item)+".id", ref_item.id) ?>
      <?cs else ?>
      <td class="radiolistcell2" width="19" align="center">
	<input type="checkbox" name="modify.mhuxd.keyer.<?cs var:unit ?>.sm.obj.<?cs var:name(item) ?>.ref.<?cs var:name(ref_item) ?>" value="1" > 
      </td>
      <?cs /if ?>


      <td class="contentlistcell" width="1%">&nbsp;</td>

      <td class="contentlistcell" colspan="3">
	<?cs call:select_link("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".ref."+name(ref_item)+".dest_id") ?>
      </td>
      
      <td class="contentlistcell" width="5%">
	RX only &nbsp;
	<?cs call:opt_bool_basic(
	     "modify.mhuxd.keyer."+unit+".sm.obj."+item.id+".ref."+ref_item.id+".rxonly", 
	     mhuxd.keyer[unit].sm.obj[item.id].ref[ref_item.id].rxonly) ?>
      </td>

      <td class="contentlistcell" width="5%">&nbsp;</td>
      <td class="contentlistcell" width="5%">&nbsp;</td>

      <td class="contentlistcell">
	<?cs if:mhuxd.keyer[unit].sm.obj[ref_item.dest_id].type == 0 ?>
	<table class="sectiontable" cellpadding="3" cellspacing="0">
	  <tr>
	    <?cs each:out_item = mhuxd.keyer[unit].sm.output ?>
	    <?cs if:mhuxd.keyer[unit].sm.output[name(out_item)] == 0 ?>
	    <td class="contentlistcell"><?cs var:name(out_item) ?></td>
	    <?cs /if ?>
	    <?cs /each ?>
	  </tr>

	  <tr>
	    <?cs each:out_item = mhuxd.keyer[unit].sm.output ?>
	    <?cs if:mhuxd.keyer[unit].sm.output[name(out_item)] == 0 ?>
	    <td class="contentlistcell"><?cs call:bool_ro(
					     "modify.mhuxd.keyer."+unit+".sm.obj."+ref_item.dest_id+".output."+name(out_item), 
					     mhuxd.keyer[unit].sm.obj[ref_item.dest_id].output[name(out_item)] ) ?></td>
	    <?cs /if ?>
	    <?cs /each ?>
	  </tr>
	</table>
	<?cs else ?>
	&nbsp;
	<?cs /if ?>
      </td>

      <td class="contentlistcell" width="1%">&nbsp;</td>
      <td class="contentlistcell" width="1%">&nbsp;</td>

    </tr>
    <?cs /each ?>

    <!-- add ref area -->

    <tr class="contentlistrow2">
      <td class="contentlistcell" width="19" align="center">&nbsp;</td>
      <td class="contentlistcell" width="1%">&nbsp;</td>


      <?cs if:mhuxd.webui.session.AddAnt[chan][item.id] ?>
      <td class="contentlistcell" colspan="3">

	<?cs call:hidden("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".type", 2) ?>
	<?cs call:select_link_rw("modify.mhuxd.keyer."+unit+".sm.obj."+name(item)+".ref.0.dest_id") ?>
      </td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>

      <td class="contentlistcell" align="center">
	<input name="Modify" onclick="button_clicked=this.value;" value="Save" type="submit">
      </td>

      <td class="contentlistcell" align="center">
	<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
      </td>

      <?cs else ?>

      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>
      <td class="contentlistcell" align="center">&nbsp;</td>


      <td class="contentlistcell"  width="25%" >
	<?cs if:subcount(mhuxd.webui.session.AddAnt[chan]) || mhuxd.webui.session.Edit[chan] ?>
	&nbsp;
	<?cs else ?>
	<input name="AddAnt.<?cs var:chan ?>.<?cs var:item.id ?>" value="Add" type="submit">
	<?cs /if ?>
      </td>

      <?cs /if ?>



    </tr>


    <?cs /if ?>
    <?cs /each ?>

    <!-- Add a new one -->
    <?cs if:mhuxd.webui.session.Add[chan] ?>
    <tr class="contentlistrow2">
      <td class="contentlistcell" width="19" align="center">&nbsp;&nbsp;</td>
      <td class="contentlistcell" width="1%" align="center">&nbsp;&nbsp;</td>

      <?cs call:hidden("set.mhuxd.keyer."+unit+".sm.obj.0.type", 2) ?>

      <td class="contentlistcell"><?cs call:string_rw(
				       "set.mhuxd.keyer."+unit+".sm.obj.0.display", 
				       "", 10 ) ?> </td>

      <td class="contentlistcell" width="25%" ><?cs call:string_rw(
				       "set.mhuxd.keyer."+unit+".sm.obj.0.low_freq", 
				       "3500000", 10 ) ?> </td>

      <td class="contentlistcell" width="25%" ><?cs call:string_rw(
				       "set.mhuxd.keyer."+unit+".sm.obj.0.high_freq", 
				       "3800000", 10 ) ?> </td>

      <td class="contentlistcell" width="25%" ><?cs call:string_rw(
				       "set.mhuxd.keyer."+unit+".sm.obj.0.bcd_code", 
				       "0", 1 ) ?> </td>

      <td class="contentlistcell"><?cs call:bool_rw(
				       "set.mhuxd.keyer."+unit+".sm.obj.0.pa_power", 
				       0 ) ?></td>

      <td class="contentlistcell"><?cs call:bool_rw(
				       "set.mhuxd.keyer."+unit+".sm.obj.0.keyout", 
				       0 ) ?></td>

      <td class="contentlistcell" width="25%" align="center">&nbsp;&nbsp;</td>
      <td class="contentlistcell" width="25%" align="center">&nbsp;&nbsp;</td>
      <td class="contentlistcell" width="25%" align="center">&nbsp;&nbsp;</td>
    </tr>
    <?cs /if ?>
    <!-- /Add new one -->

  </tbody>
</table>
<img src="/static/dot.gif" height="4" border="0" width="1"><br>



<?cs /def ?>

<?cs def:sm_antsw_band_list(unit, chan) ?>
<?cs call:sm_antsw_band_list1(mhuxd.webui.session.unit, chan) ?>

<?cs if:subcount(mhuxd.webui.session.AddAnt[chan]) ?>
<?cs elseif:mhuxd.webui.session.Add[chan] ?>
<input name="SaveButton" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs elseif:mhuxd.webui.session.Edit[chan] ?>
<input name="Modify" onclick="button_clicked=this.value;" value="Save" type="submit">
<input name="CancelButton" onclick="button_clicked=this.value;" value="Cancel" type="submit">
<?cs else ?>
<input name="Add.<?cs var:chan ?>" value="Add" type="submit">
<input name="Edit.<?cs var:chan ?>" value="Edit" type="submit">
<input name="Remove" value="Remove" type="submit">
<?cs /if ?>
<?cs /def ?>

<?cs call:sm_antsw_band_list(unit, "sm_antsw_band_list") ?>
