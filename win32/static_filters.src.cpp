/*File generated during static filter build
  Automatically generated file
*/

  extern "C" IndividualFilter * new_aspell_context_filter();

  extern "C" IndividualFilter * new_aspell_texinfo_filter();

  extern "C" IndividualFilter * new_aspell_sgml_decoder();

  extern "C" IndividualFilter * new_aspell_sgml_filter();

  extern "C" IndividualFilter * new_aspell_tex_filter();

  extern "C" IndividualFilter * new_aspell_url_filter();

  extern "C" IndividualFilter * new_aspell_nroff_filter();

  extern "C" IndividualFilter * new_aspell_email_filter();

  extern "C" IndividualFilter * new_aspell_html_decoder();

  extern "C" IndividualFilter * new_aspell_html_filter();

  static FilterEntry standard_filters[] = {
    {"context",0,new_aspell_context_filter,0},
    {"texinfo",0,new_aspell_texinfo_filter,0},
    {"sgml",new_aspell_sgml_decoder,new_aspell_sgml_filter,0},
    {"tex",0,new_aspell_tex_filter,0},
    {"url",0,new_aspell_url_filter,0},
    {"nroff",0,new_aspell_nroff_filter,0},
    {"email",0,new_aspell_email_filter,0},
    {"html",new_aspell_html_decoder,new_aspell_html_filter,0}
  };

  const unsigned int standard_filters_size = sizeof(standard_filters)/sizeof(FilterEntry);

  static KeyInfo context_options[] = {
    {
      "f-context-visible-first",
      KeyInfoBool,
      "false",
      "swaps visible and invisible text"
    },
    {
      "f-context-delimiters",
      KeyInfoList,
      "\" \":/* */:// 0",
      "context delimiters (separated by spaces)"
    }
  };

  const KeyInfo * context_options_begin = context_options;

  const KeyInfo * context_options_end = context_options+sizeof(context_options)/sizeof(KeyInfo);

  static KeyInfo texinfo_options[] = {
    {
      "f-texinfo-ignore-env",
      KeyInfoList,
      "example:smallexample:verbatim:lisp:smalllisp:small:display:snalldisplay:format:smallformat",
      "Texinfo environments to ignore"
    },
    {
      "f-texinfo-ignore",
      KeyInfoList,
      "setfilename:syncodeindex:documentencoding:vskip:code:kbd:key:samp:verb:var:env:file:command:option:url:uref:email:verbatiminclude:xref:ref:pxref:inforef:c",
      "Texinfo commands to ignore the parameters of"
    }
  };

  const KeyInfo * texinfo_options_begin = texinfo_options;

  const KeyInfo * texinfo_options_end = texinfo_options+sizeof(texinfo_options)/sizeof(KeyInfo);

  static KeyInfo sgml_options[] = {
    {
      "f-sgml-skip",
      KeyInfoList,
      "",
      "SGML tags to always skip the contents of"
    },
    {
      "f-sgml-check",
      KeyInfoList,
      "",
      "SGML attributes to always check"
    }
  };

  const KeyInfo * sgml_options_begin = sgml_options;

  const KeyInfo * sgml_options_end = sgml_options+sizeof(sgml_options)/sizeof(KeyInfo);

  static KeyInfo tex_options[] = {
    {
      "f-tex-command",
      KeyInfoList,
      "addtocounter pp:addtolength pp:alpha p:arabic p:fnsymbol p:roman p:stepcounter p:setcounter pp:usecounter p:value p:newcounter po:refstepcounter p:label p:pageref p:ref p:newcommand poOP:renewcommand poOP:newenvironment poOPP:renewenvironment poOPP:newtheorem poPo:newfont pp:documentclass op:usepackage op:begin po:end p:setlength pp:addtolength pp:settowidth pp:settodepth pp:settoheight pp:enlargethispage p:hyphenation p:pagenumbering p:pagestyle p:addvspace p:framebox ooP:hspace p:vspace p:makebox ooP:parbox ooopP:raisebox pooP:rule opp:sbox pO:savebox pooP:usebox p:include p:includeonly p:input p:addcontentsline ppP:addtocontents pP:fontencoding p:fontfamily p:fontseries p:fontshape p:fontsize pp:usefont pppp:documentstyle op:cite p:nocite p:psfig p:selectlanguage p:includegraphics op:bibitem op:geometry p",
      "TeX commands"
    },
    {
      "f-tex-check-comments",
      KeyInfoBool,
      "false",
      "check TeX comments"
    }
  };

  const KeyInfo * tex_options_begin = tex_options;

  const KeyInfo * tex_options_end = tex_options+sizeof(tex_options)/sizeof(KeyInfo);

  static KeyInfo url_options[] = {0

  };

  const KeyInfo * url_options_begin = url_options;

  const KeyInfo * url_options_end = url_options;//+sizeof(url_options)/sizeof(KeyInfo);

  static KeyInfo nroff_options[] = {0

  };

  const KeyInfo * nroff_options_begin = nroff_options;

  const KeyInfo * nroff_options_end = nroff_options;//+sizeof(nroff_options)/sizeof(KeyInfo);

  static KeyInfo email_options[] = {
    {
      "f-email-margin",
      KeyInfoInt,
      "10",
      "num chars that can appear before the quote char"
    },
    {
      "f-email-quote",
      KeyInfoList,
      ">:|",
      "email quote characters"
    }
  };

  const KeyInfo * email_options_begin = email_options;

  const KeyInfo * email_options_end = email_options+sizeof(email_options)/sizeof(KeyInfo);

  static KeyInfo html_options[] = {
    {
      "f-html-skip",
      KeyInfoList,
      "script:style",
      "HTML tags to always skip the contents of"
    },
    {
      "f-html-check",
      KeyInfoList,
      "alt",
      "HTML attributes to always check"
    }
  };

  const KeyInfo * html_options_begin = html_options;

  const KeyInfo * html_options_end = html_options+sizeof(html_options)/sizeof(KeyInfo);


  static ConfigModule filter_modules[] = {
    {
      "context",0,
      "experimental filter for hiding delimited contexts",
      context_options_begin,context_options_end
    },
    {
      "texinfo",0,
      "filter for dealing with Texinfo documents",
      texinfo_options_begin,texinfo_options_end
    },
    {
      "sgml",0,
      "filter for dealing with generic SGML/XML documents",
      sgml_options_begin,sgml_options_end
    },
    {
      "tex",0,
      "filter for dealing with TeX/LaTeX documents",
      tex_options_begin,tex_options_end
    },
    {
      "url",0,
      "filter to skip URL like constructs",
      url_options_begin,url_options_end
    },
    {
      "nroff",0,
      "filter for dealing with Nroff documents",
      nroff_options_begin,nroff_options_end
    },
    {
      "email",0,
      "filter for skipping quoted text in email messages",
      email_options_begin,email_options_end
    },
    {
      "html",0,
      "filter for dealing with HTML documents",
      html_options_begin,html_options_end
    }
  };

  const ConfigModule * filter_modules_begin = filter_modules;

  const ConfigModule * filter_modules_end = filter_modules+sizeof(filter_modules)/sizeof(ConfigModule);

  const size_t filter_modules_size = sizeof(filter_modules);
