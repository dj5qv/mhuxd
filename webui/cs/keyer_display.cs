<?cs include:"display_cfg.cs" ?>
<?cs include:"display_ev.cs" ?>
<?cs call:sectionheader("Display Options", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:display_cfg(mhuxd.webui.session.unit, "r1") ?>
<br><br>

<?cs call:sectionheader("Display Reports", "foobar_help") ?>
&nbsp;&nbsp;<br>
<?cs call:display_ev(mhuxd.webui.session.unit, "r1rep") ?>
<br><br>
