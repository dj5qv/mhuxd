


	  <table class="left-menu-table" border="0" cellpadding="0" cellspacing="0">
	    <tbody>

	      <?cs each:item = mhuxd.webui.menus[mhuxd.webui.session.page] ?>
	      <?cs if:!item.depends || mhuxd.run.keyer[mhuxd.webui.session.unit].flags[item.depends] == 1 ?>

	      <tr>
		<td class=
		    <?cs if:mhuxd.webui.session.menu == name(item) ?>   
		    "menuselectedcell"   
		    <?cs else ?> 
		    "menucell" 
		    <?cs /if ?> >
		  <table>
		    <tbody>
		      <tr>
			<td>
			  <a href="<?cs var:mhuxd.webui.base_url ?>?page=<?cs var:mhuxd.webui.session.page ?>&unit=<?cs var:mhuxd.webui.session.unit ?>&menu=<?cs var:name(item) ?>" 
			     class="menulink"><img src="/static/big_bullet.gif" alt="" border="0" height="20" width="20"></a>
			</td>
			<td class="submenuselectedcelltext">
			  <a href="<?cs var:mhuxd.webui.base_url ?>?page=<?cs var:mhuxd.webui.session.page ?>&unit=<?cs var:mhuxd.webui.session.unit ?>&menu=<?cs var:name(item) ?>" 
			     class="menulink"><?cs var:item.display ?></a>
			</td>
		      </tr>
		    </tbody>
		  </table>
		</td>
	      </tr>

	      <?cs /if ?>
	      <?cs /each ?>

	      <tr>
		<td><img src="/static/dot.gif" alt="" height="5" width="1"></td>
	      </tr>

	    </tbody>
	  </table>
  <br>
  <br>
</td>
