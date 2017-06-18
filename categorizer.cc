#include "categorizer.h"

Category Categorizer::Categorize(const QString &file_ext) {
  QString lower_file_ext = file_ext.toLower();
  if (videos_.contains(lower_file_ext)) {
    return Category::VIDEOS;
  } else if (images_.contains(lower_file_ext)) {
    return Category::IMAGES;
  } else if (audio_.contains(lower_file_ext)) {
    return Category::AUDIO;
  } else if (documents_.contains(lower_file_ext)) {
    return Category::DOCUMENTS;
  } else if (software_.contains(lower_file_ext)) {
    return Category::SOFTWARE;
  } else {
    return Category::OTHER;
  }
}

const QSet<QString> Categorizer::videos_ = {
  "webm",
  "mkv",
  "flv",
  "vob",
  "drc",
  "mng",
  "avi",
  "mov",
  "qt",
  "wmv",
  "yuv",
  "rm",
  "rmvb",
  "asf",
  "mp4",
  "m4p",
  "mpg",
  "mp2",
  "mpeg",
  "mpe",
  "mpv",
  "mpg",
  "m2v",
  "m4v",
  "svi",
  "3gp",
  "3g2",
  "mxf",
  "roq",
  "nsv",
};

const QSet<QString> Categorizer::audio_ = {
  "act",
  "aiff",
  "aac",
  "amr",
  "ape",
  "au",
  "awb",
  "dct",
  "dss",
  "dvf",
  "flac",
  "gsm",
  "iklax",
  "ivs",
  "m4a",
  "m4p",
  "mmf",
  "mp3",
  "mpc",
  "msv",
  "ogg,",
  "opus",
  "ra,",
  "raw",
  "sln",
  "tta",
  "vox",
  "wav",
  "wma",
  "wv",
  "webm",
};

const QSet<QString> Categorizer::images_ = {
  "tiff",
  "tif",
  "gif",
  "jpeg",
  "jpg",
  "jif",
  "jfif",
  "jp2",
  "jpx",
  "j2k",
  "j2c",
  "fpx",
  "pcd",
  "png",
  "svg",
};

const QSet<QString> Categorizer::documents_ = {
  "doc",
  "docx",
  "rtf",
  "txt",
  "wpd",
  "wps",
  "csv",
  "pps",
  "ppt",
  "pptx",
  "xml",
  "xls",
  "xlsx",
  "mdb",
  "db",
  "accdb",
  "pdf"
};

const QSet<QString> Categorizer::software_ = {
  "app",
  "bat",
  "cgi",
  "com",
  "exe",
  "jar",
  "dmg",
  "iso",
  "img",
  "nrg",
  "vmdk"
};
