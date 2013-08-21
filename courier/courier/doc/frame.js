myname=self.location.href;

x=top.location.href;

if (x != myname)
{
   top.location=myname;
}

if (x.substr(0, 5) != "file:")
{
  document.write('<object id="pmenu" data="menu.html" style="z-index: 1;');
}
else
{
  document.write('<object id="pmenu" style="z-index: 1;');
}

document.write('position: fixed;');
document.write('left: 0px; top: 30px;');
document.write('width: 100%; height: 100%;');
document.write('visibility: hidden" ></object>');

function menuon()
{
   var p=document.getElementById('pmenu');

   if (p.style.visibility == 'visible')
   {
       p.style.visibility='hidden';
   }
   else
   {
       p.style.visibility='visible';
   }
}
