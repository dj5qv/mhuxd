<?cs include:"winkey_cfg.cs" ?>
<?cs include:"sidetone_cfg.cs" ?>


<?cs call:sectionheader("Winkey", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:winkey_cfg(mhuxd.webui.session.unit, "wk") ?>
<br>

<?cs call:sectionheader("Side Tone", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:sidetone_cfg(mhuxd.webui.session.unit, "sidetone", mhuxd.run.keyer[mhuxd.webui.session.unit].info.type) ?>
<br><br>

