
var platformMoz = (document.implementation && document.implementation.createDocument);
var platformIE6 = (!platformMoz && document.getElementById && window.ActiveXObject);
var noXSLT      = (!platformMoz && !platformIE6);

var msxmlVersion = '3.0';
var urlXML;
var urlXSL;
var docXML;
var docXSL;
var target;
var cache;;
var processor;
var i;
var DefaultTreeMode;


if (platformIE6)
{
    cache = new ActiveXObject('Msxml2.XSLTemplate.' + msxmlVersion);
}


function initializeTree(defTreeMode)
{
    if (noXSLT)
    {
        alert("Sorry, this doesn't work in your browser");
        return;
    }
   
    urlXML = get_report_filename()
    urlXSL = "tree/tree.xsl";
    target = document.getElementById("xmlContent");
    
	DefaultTreeMode = defTreeMode;
    Transform();
}

function get_report_filename()
{
    var filename;
    var i;
    var j;
    var c;
    var fileId;

    filename = document.URL;
    j = filename.length;
    k = 0
    for (i=filename.length-1; i>=0; i--)
    {
        
        c = filename.charAt(i)
        
        if ( (j == filename.length) && (c == '.') )
        {
            j = i;
        }
        if ( (c == '/') || (c == '\\') )
        {
            break;
        }
    }
    if (i != -1)
    {
        return 'xml/' + filename.substring(i+1,j) + '.xml'
    }
    else
    {
        return 'xml/report.xml'
    }
}


function CreateDocument()
{
    var doc = null;

    if (platformMoz)
    {
        doc = document.implementation.createDocument('', '', null);
    }
    else if (platformIE6)
    {
        doc = new ActiveXObject('Msxml2.FreeThreadedDOMDocument.' + msxmlVersion);
    }
    return doc;
}

function Transform() 
{
    

    docXML = CreateDocument();
    docXSL = CreateDocument();
    if (platformMoz)
    {
        docXML.addEventListener('load', DoLoadXSL, false);
        docXML.load(urlXML);
    }
    else if (platformIE6)
    {
        docXML.async = false;
        docXML.load(urlXML);
        docXSL.async = false;
        docXSL.load(urlXSL);
        DoTransform();
    }
}

function DoLoadXSL()
{
  if (platformMoz)
  {
      docXSL.addEventListener('load', DoTransform, false);
      docXSL.load(urlXSL);
  }
}

function DoTransform() 
{
    if (platformMoz)
    {
        processor = new XSLTProcessor();
        processor.importStylesheet(docXSL);

        processor.setParameter(null, "DefaultTreeMode", DefaultTreeMode);
        
        var fragment = processor.transformToFragment(docXML, document);
        while (target.hasChildNodes())
        {
            target.removeChild(target.childNodes[0]);
        }
        target.appendChild(fragment);
    }
    else if (platformIE6)
    {
        cache.stylesheet = docXSL;

        processor = cache.createProcessor();
        processor.input = docXML;
        processor.addParameter("DefaultTreeMode", DefaultTreeMode);
        
        processor.transform();
        target.innerHTML = processor.output;
    }
    
}

//----------------------------------------------------
function cancelBuble(event)
{
    if (window.event)
    {
        window.event.cancelBubble = true;
        window.event.returnValue = false;
    }
    else if (event && event.preventDefault && event.stopPropagation)
    {
        event.preventDefault();
        event.stopPropagation();
    }    
}

//----------------------------------------------------
function clickOnEntity(event, entity)
{
    // cancel buble    
    cancelBuble(event)

    if(entity.getAttribute("open") == "false")
    {
        expand(entity)
    }
    else
    {
        collapse(entity)
    }
    
    // cancel buble
    cancelBuble(event)
    
}

//----------------------------------------------------
function expand(entity)
{
    // Variable declarations
    var oImage
    var i
    
    
    // Get class name
    if (platformMoz)
        cl = entity.getAttribute("CLASS");
    else if (platformIE6)
        cl = entity.className

    // Get and change image
    if (cl == "item")
    {
        oImage = entity.childNodes[0].childNodes[0].childNodes[0].childNodes[0].childNodes[0]        
        oImage.src = entity.getAttribute("openimage")
    }
    
    for(i=0; i < entity.childNodes.length; i++)
    {
        node = entity.childNodes[i]
        if((node.tagName == "DIV") || (node.tagName == "div"))
        {
            // Display child
            node.style.display = "block"
        }
    }
    entity.setAttribute("open","true")
}

//----------------------------------------------------
function collapse(entity)
{
    
    // Variable declarations
    var oImage
    var i

    // Get class name
    if (platformMoz)
        cl = entity.getAttribute("CLASS");
    else if (platformIE6)
        cl = entity.className

    // Get and change image
    if (cl == "item")
    {
        oImage = entity.childNodes[0].childNodes[0].childNodes[0].childNodes[0].childNodes[0]
        oImage.src = entity.getAttribute("closeimage")
    }
    for(i=0; i < entity.childNodes.length; i++)
    {
        node = entity.childNodes[i]
        if((node.tagName == "DIV") || (node.tagName == "div"))
        {
            // Display child
            node.style.display = "none"
        }
    }
    
    entity.setAttribute("open","false")
}

//----------------------------------------------------
function expandAllFromString(entityString)
{
    entity = document.getElementById(entityString);
    expandAll(entity, 1)
}

function expandAll(entity, isRoot)
{
    var i
    // expand current node
    expand(entity)

    // expand children
    for(i=0; i < entity.childNodes.length; i++)
    {
        if ((entity.childNodes[i].tagName == "DIV") || (entity.childNodes[i].tagName == "div"))
        {
            expandAll(entity.childNodes[i], 0)
        }
    }
}

//----------------------------------------------------

function collapseAllFromString(entityString)
{
    entity = document.getElementById(entityString);
    collapseAll(entity, 1)
}

function collapseAll(entity, isRoot)
{

    var i
    // collapse current node
    idStr = entity.id
    if ( isRoot == 0 )
    {
        collapse(entity)
    }
    // expand children
    for(i=0; i < entity.childNodes.length; i++)
    {
        if((entity.childNodes[i].tagName == "DIV") || (entity.childNodes[i].tagName == "div"))
        {
            collapseAll(entity.childNodes[i], 0)
        }
    }
}