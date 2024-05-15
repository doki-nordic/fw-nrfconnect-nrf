
import os
import re
import sys
import json
from pathlib import Path
from github import Github
from types import SimpleNamespace
from jinja2 import Template
from github.Repository import Repository
from github.PullRequest import PullRequest
from github.IssueComment import IssueComment

API_CHECK_COMMENT_INDICATOR = '<!-- API-check comment -->'

github: Github
repo: Repository
github_actor: str
pr: PullRequest

class Stats(SimpleNamespace):
    notice: int
    warning: int
    critical: int
    def __init__(self, file: os.PathLike):
        with open(file, 'r') as fd:
            dict = json.load(fd)
        super().__init__(**dict)

def fatal(*args, **kwargs):
    print(*args, **kwargs, file=sys.stderr)
    sys.exit(1)

def get_stats() -> Stats:
    stats: 'Stats | None' = None
    for arg in sys.argv[1:]:
        if not Path(arg).exists():
            fatal(f'The "{arg}" does not exist. Probably checking script failed.')
        file_stats = Stats(arg)
        if stats:
            stats.notice += file_stats.notice
            stats.warning += file_stats.warning
            stats.critical += file_stats.critical
        else:
            stats = file_stats
    if stats is None:
        fatal('No input files.')
    return stats

def get_message(stats: Stats) -> str:
    template_path: Path = Path(__file__).parent / 'template.md.jinja'
    template = Template(template_path.read_text())
    message = API_CHECK_COMMENT_INDICATOR + '\n' + template.render(**stats.__dict__).strip()
    return message

def get_meta(message, keyword) -> list[str]:
    result = []
    for match in re.finditer(r'<!--\s*' + keyword + r':\s*(.*?)\s*-->', message, re.DOTALL):
        result.append(match.group(1))
    return result

def main():
    global github, github_actor, repo, pr

    stats = get_stats()
    print('Stats', stats)

    message = get_message(stats)
    print(f'Comment message:\n{message}\n------------------------------------')
    exit()

    github = Github(os.environ['GITHUB_TOKEN'])
    print(f'Github API connected. Remaining requests {github.rate_limiting[0]} of {github.rate_limiting[1]}.')

    github_actor = os.environ['GITHUB_ACTOR']
    print(f'Github user: {github_actor}')

    repo = github.get_repo(os.environ['GITHUB_REPO'], lazy=True)
    pr = repo.get_pull(int(os.environ['PR_NUMBER']))
    print(f'Pull request: {pr.title} #{pr.number} {pr.html_url}')

    comment: 'IssueComment | None'
    for comment in pr.get_issue_comments():
        if comment.body.strip().startswith(API_CHECK_COMMENT_INDICATOR):
            if message == comment.body:
                print(f'Comment unchanged: {comment.html_url}')
            else:
                print(f'Editing comment: {comment.html_url}')
                comment.edit(message)
            break
    else:
        print(f'Adding new comment.')
        comment = pr.create_issue_comment(message)
        print(f'Added comment: {comment.html_url}')

    labels = get_meta(message, 'add-label')
    if len(labels) > 0:
        print(f'Adding labels: {", ".join(labels)}')
        pr.add_to_labels(*labels)

    for label in get_meta(message, 'remove-label'):
        print(f'Removing label: {label}')
        for existing_label in pr.labels:
            if existing_label.name == label:
                pr.remove_from_labels(label)
                break
        else:
            print(f'Label already removed: {label}')

    exit_code = 0
    for value in get_meta(message, 'exit-code'):
        exit_code = int(value)
    sys.exit(exit_code)


if __name__ == '__main__':
    main()
