// Copyright (c) 2023 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
import styled from 'styled-components';
import Link from 'next/link';

export const BlogCtn = styled.div`
    padding: 160px 0;
    background-size: 60px 103px;
    background-image: url(/images/logo-tile.png);
    ${props => props.theme.filters.grayscale}
    ${props => props.theme.breakpoint.medium} {
        padding: 120px 0;
    }
`;

export const FeaturedCardCtn = styled.div`
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    grid-template-rows: repeat(2, 1fr);
    grid-column-gap: 15px;
    grid-row-gap: 15px;
    ${props => props.theme.breakpoint.medium} {
        grid-template-columns: repeat(1, 1fr);
        grid-template-rows: repeat(1, 1fr);
    }
`;

export const TextCtn = styled.div`
    padding: 30px;
    color: ${props => props.theme.colors.contrast};
    transition: all ease-in-out 300ms;
    flex-grow: 1;
    align-items: center;
    background-color: ${props => props.theme.colors.darkBackground};
    h2,
    h3,
    h4 {
        margin: 0;
        ${props => props.theme.breakpoint.medium} {
            font-size: 18px;
            line-height: 1.3em;
        }
    }
    p {
        margin: 0;
        margin-top: 7px;
        font-size: 16px;
        line-height: 1.8em;
        opacity: 0.5;
        ${props => props.theme.breakpoint.medium} {
            font-size: 14px;
        }
    }
    ${props => props.theme.breakpoint.medium} {
        padding: 15px;
    }
`;

export const CardImage = styled.div`
    position: relative;
    height: ${props => (props.feature ? '500px' : '200px')};
    width: 100%;
    overflow: hidden;
    img {
        object-fit: cover;
        object-position: center;
        transition: all ease-in-out 300ms;
    }
    ${props => props.theme.breakpoint.medium} {
        height: ${props => (props.feature ? '300px' : '180px')};
    }
`;

export const FeaturedCard = styled(Link)`
    grid-area: 1 / 1 / 3 / 3;
    display: flex;
    flex-direction: column;
    border: 1px solid ${props => props.theme.colors.primaryLight};
    transition: all ease-in-out 200ms;
    position: relative;
    box-shadow: 0px 0px 22px 1px ${props => props.theme.colors.primaryLight}68;
    :nth-child(2) {
        grid-area: 1 / 3 / 2 / 4;
    }
    :nth-child(3) {
        grid-area: 2 / 3 / 3 / 4;
    }
    :hover {
        border-color: ${props => props.theme.colors.accent};
        ${TextCtn} {
            background-color: ${props => props.theme.colors.contrast};
            color: ${props => props.theme.colors.black};
        }
        ${CardImage} {
            img {
                transform: scale(1.2);
            }
        }
    }
    ${props => props.theme.breakpoint.medium} {
        grid-area: unset !important;
    }
`;

export const Tag = styled.div`
    padding: 5px 6px 5px 10px;
    background: ${props => props.theme.colors.accent};
    box-shadow: -4px -4px 0 0 ${props => props.theme.colors.accent}46;
    font-size: 10px;
    line-height: 1;
    font-weight: 500;
    text-align: center;
    letter-spacing: 3px;
    text-transform: uppercase;
    color: ${props => props.theme.colors.contrast};
    display: inline-block;
    align-self: end;
    position: absolute;
    top: 15px;
    right: 15px;
    z-index: 99;
`;

export const CardCtn = styled.div`
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    grid-template-rows: repeat(1, 1fr);
    grid-column-gap: 15px;
    grid-row-gap: 15px;
    margin-top: 15px;
    ${props => props.theme.breakpoint.medium} {
        grid-template-columns: repeat(2, 1fr);
    }
    ${props => props.theme.breakpoint.small} {
        grid-template-columns: repeat(1, 1fr);
    }
`;

export const Card = styled(Link)`
    display: flex;
    flex-direction: column;
    border: 1px solid ${props => props.theme.colors.primaryLight};
    transition: all ease-in-out 200ms;
    position: relative;
    box-shadow: 0px 0px 22px 1px ${props => props.theme.colors.primaryLight}68;
    :hover {
        border-color: ${props => props.theme.colors.accent};
        ${TextCtn} {
            background-color: ${props => props.theme.colors.contrast};
            color: ${props => props.theme.colors.black};
        }
        ${CardImage} {
            img {
                transform: scale(1.2);
            }
        }
    }
    ${TextCtn} {
        padding: 15px;
    }
    ${CardImage} {
        height: 150px;
    }
`;

export const DateText = styled.div`
    color: ${props => props.theme.colors.primaryLight};
    font-size: 14px;
    ${props => props.theme.breakpoint.medium} {
        font-size: 12px;
    }
`;
